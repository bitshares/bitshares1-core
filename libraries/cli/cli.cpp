#include <bts/cli/cli.hpp>
#include <bts/rpc/rpc_server.hpp>

#include <fc/io/buffered_iostream.hpp>
#include <fc/io/console.hpp>
#include <fc/io/json.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifndef WIN32
#define HAVE_READLINE
#endif

#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

namespace bts { namespace cli {

  namespace detail
  {
      class cli_impl
      {
         public:
            client_ptr                  _client;
            rpc_server_ptr              _rpc_server;
            bts::cli::cli*              _self;
            fc::thread*                 _main_thread;
            fc::thread                  _cin_thread;
            fc::future<void>            _cin_complete;
            std::set<std::string>       _json_command_list;

            cli_impl( const client_ptr& client, const rpc_server_ptr& rpc_server );

            void process_commands()
            {
              std::string line = _self->get_line();
              while (std::cin.good())
              {
                std::string trimmed_line_to_parse(boost::algorithm::trim_copy(line));
                if (!trimmed_line_to_parse.empty())
                {
                  std::string::const_iterator iter = std::find_if(trimmed_line_to_parse.begin(), trimmed_line_to_parse.end(), ::isspace);
                  std::string command;
                  fc::istream_ptr argument_stream;
                  if (iter != trimmed_line_to_parse.end())
                  {
                    // then there are arguments to this function
                    size_t first_space_pos = iter - trimmed_line_to_parse.begin();
                    command = trimmed_line_to_parse.substr(0, first_space_pos);
                    argument_stream = std::make_shared<fc::stringstream>((trimmed_line_to_parse.substr(first_space_pos + 1)));
                  }
                  else
                  {
                    command = trimmed_line_to_parse;
                    argument_stream = std::make_shared<fc::stringstream>();
                  }

                  fc::buffered_istream buffered_argument_stream(argument_stream);

                  bool command_is_valid = false;
                  fc::variants arguments;
                  try
                  {
                    arguments = _self->parse_interactive_command(buffered_argument_stream, command);
                    command_is_valid = true;
                  }
                  catch (fc::key_not_found_exception&)
                  {
                    std::cout << "Error: invalid command \"" << command << "\"\n";
                  }
                  catch (fc::exception& e)
                  {
                    std::cout << "Error parsing command \"" << command << "\": " << e.to_string() << "\n";
                  }

                  if (command_is_valid)
                  {
                    try
                    {
                      fc::variant result = _self->execute_interactive_command(command, arguments);
                      _self->format_and_print_result(command, result);
                    }
                    catch ( const fc::canceled_exception& )
                    {
                      return;
                    }
                    catch ( const fc::exception& e )
                    {
                      std::cout << e.to_detail_string() << "\n";
                    }
                  }
                }
                line = _self->get_line();
              }
            }

            std::string get_line( const std::string& prompt, bool no_echo )
            {
                  std::string line;
                  if ( no_echo )
                  {
                      // there is no need to add input to history when echo is off, so both Windows and Unix implementations are same
                      std::cout<<prompt;
                      fc::set_console_echo(false);
                      std::getline( std::cin, line );
                      fc::set_console_echo(true);
                      std::cout << std::endl;
                  }
                  else
                  {
                  #ifdef HAVE_READLINE
                     char* line_read = nullptr;
                     line_read = readline(prompt.c_str());
                     if(line_read && *line_read)
                         add_history(line_read);
                     if( line_read == nullptr )
                        FC_THROW_EXCEPTION( eof_exception, "" );
                     line = line_read;
                     free(line_read);
                  #else
                     std::cout<<prompt;
                     std::getline( std::cin, line );
                  #endif
                  }

                  boost::trim(line);
                  return line;
            }

            fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command)
            {
              try
              {
                const rpc_server::method_data& method_data = _rpc_server->get_method_data(command);
                return _self->parse_recognized_interactive_command(argument_stream, method_data);
              }
              catch (fc::key_not_found_exception&)
              {
                return _self->parse_unrecognized_interactive_command(argument_stream, command);
              }
            }

            fc::variants parse_recognized_interactive_command( fc::buffered_istream& argument_stream,
                                                               const rpc_server::method_data& method_data)
            {
              fc::variants arguments;
              for (unsigned i = 0; i < method_data.parameters.size(); ++i)
              {
                try
                {
                  arguments.push_back(_self->parse_argument_of_known_type(argument_stream, method_data, i));
                }
                catch (const fc::eof_exception& e)
                {
                  if (method_data.parameters[i].classification != rpc_server::required_positional)
                    return arguments;
                  else
                    FC_THROW("Missing argument ${argument_number} of command \"${command}\"",
                             ("argument_number", i + 1)("command", method_data.name)("cause",e.to_detail_string()) );
                }
                catch (fc::parse_error_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", i + 1)("command", method_data.name)("detail", e.get_log()));
                }

                if (method_data.parameters[i].classification == rpc_server::optional_named)
                  break;
              }
              return arguments;
            }

            fc::variants parse_unrecognized_interactive_command( fc::buffered_istream& argument_stream,
                                                                 const std::string& command)
            {
              /* Quit isn't registered with the RPC server, the RPC server spells it "stop" */
              if (command == "quit")
                return fc::variants();

              FC_THROW_EXCEPTION(key_not_found_exception, "Unknown command \"${command}\".", ("command", command));
            }

            fc::variant parse_argument_of_known_type( fc::buffered_istream& argument_stream,
                                                      const rpc_server::method_data& method_data,
                                                      unsigned parameter_index)
            {
              const rpc_server::parameter_data& this_parameter = method_data.parameters[parameter_index];
              if (this_parameter.type == "asset")
              {
                // for now, accept plain int, assume it's always in the base asset
                uint64_t amount_as_int;
                try
                {
                  fc::variant amount_as_variant = fc::json::from_stream(argument_stream);
                  amount_as_int = amount_as_variant.as_uint64();
                }
                catch ( fc::bad_cast_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
                catch ( fc::parse_error_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
                return fc::variant(bts::blockchain::asset(amount_as_int));
              }
              else if (this_parameter.type == "address")
              {
                // allow addresses to be un-quoted
                while( isspace(argument_stream.peek()) )
                  argument_stream.get();
                fc::stringstream address_stream;
                try
                {
                  while( !isspace(argument_stream.peek()) )
                    address_stream.put(argument_stream.get());
                }
                catch( const fc::eof_exception& )
                {
                   // expected and ignored
                }
                std::string address_string = address_stream.str();

                try
                {
                  bts::blockchain::address::is_valid(address_string);
                }
                catch ( fc::exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
                return fc::variant( bts::blockchain::address(address_string) );
              }
              else
              {
                // assume it's raw JSON
                try
                {
                  return fc::json::from_stream( argument_stream );
                }
                catch( fc::parse_error_exception& e )
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
              }
            }

            fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments)
            {
              if (command == "wallet_create" || command == "wallet_change_passphrase")
              {
                  if( command == "wallet_create" )
                  {
                      auto wallet_name = arguments[0].as_string();
                      if( fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                      {
                        std::cout << "Wallet \"" << wallet_name << "\" already exists\n";
                        return fc::variant(false);
                      }
                  }
                  try
                  {
                      return execute_wallet_command_with_passphrase_query( command, arguments, "new passphrase", true );
                  }
                  catch (const fc::canceled_exception&)
                  {
                      std::cout << "Command aborted\n";
                      return fc::variant( false );
                  }
              }
              else if (command == "wallet_create_from_json")
              {
                  auto filename = arguments[0].as<fc::path>();
                  auto wallet_name = arguments[1].as_string();
                  if( !fc::exists( filename ) )
                  {
                     std::cout << "File \"" << filename.generic_string() << "\" not found\n";
                     return fc::variant(false);
                  }
                  if( fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                  {
                    std::cout << "Wallet \"" << wallet_name << "\" already exists\n";
                    return fc::variant(false);
                  }
                  try
                  {
                      return execute_wallet_command_with_passphrase_query( command, arguments, "imported wallet passphrase" );
                  }
                  catch (const fc::canceled_exception&)
                  {
                      std::cout << "Command aborted\n";
                      return fc::variant( false );
                  }
              }
              else if ( command == "wallet_open" || command == "wallet_open_file" || command == "wallet_unlock")
              {
                  if( command == "wallet_open" )
                  {
                      auto wallet_name = arguments[0].as_string();
                      if( !fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                      {
                        std::cout << "Wallet \"" << wallet_name << "\" not found\n";
                        return fc::variant(false);
                      }
                  }
                  else if( command == "wallet_open_file" )
                  {
                      auto filename = arguments[0].as<fc::path>();
                      if( !fc::exists( filename ) )
                      {
                         std::cout << "File \"" << filename.generic_string() << "\" not found\n";
                         return fc::variant(false);
                      }
                  }
                  try
                  {
                      return execute_wallet_command_with_passphrase_query( command, arguments, "passphrase" );
                  }
                  catch (const fc::canceled_exception&)
                  {
                      std::cout << "Command aborted\n";
                      return fc::variant( false );
                  }
              }
              else if (command == "wallet_import_bitcoin")
              {
                  auto filename = arguments[0].as<fc::path>();
                  if( !fc::exists( filename ) )
                  {
                     std::cout << "File \"" << filename.generic_string() << "\" not found\n";
                     return fc::variant(false);
                  }
                  try /* Try empty password first */
                  {
                      auto new_arguments = arguments;
                      new_arguments.push_back( fc::variant( "" ) );
                      return _rpc_server->direct_invoke_method( command, new_arguments );
                  }
                  catch( const fc::exception& e )
                  {
                     ilog( "failed with empty password: ${e}", ("e",e.to_detail_string() ) );
                  }
                  try
                  {
                      return execute_wallet_command_with_passphrase_query( command, arguments, "imported wallet passphrase" );
                  }
                  catch (const fc::canceled_exception&)
                  {
                      std::cout << "Command aborted\n";
                      return fc::variant( false );
                  }
              }
              else if(command == "quit")
              {
                FC_THROW_EXCEPTION(canceled_exception, "quit command issued");
              }

              return execute_command(command, arguments);
            }

            fc::variant execute_command(const std::string& command, const fc::variants& arguments)
            { try {
              while (true)
              {
                // try to execute the method.  If the method needs the wallet to be
                // unlocked, it will throw an exception, and we'll attempt to
                // unlock it and then retry the command
                try
                {
                    return _rpc_server->direct_invoke_method(command, arguments);
                }
                catch (const rpc_wallet_open_needed_exception&)
                {
                    auto result = interactive_open_wallet();
                    if( !result.as_bool() ) return result;
                }
                catch (const rpc_wallet_unlock_needed_exception&)
                {
                    fc::variants arguments { 60 * 5 }; // default to five minute timeout
                    auto result = execute_interactive_command( "wallet_unlock", arguments );
                    if( !result.as_bool() ) return result;
                }
              }
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",command) ) }

            /** assumes last argument is passphrase */
            fc::variant execute_command_with_passphrase_query( const std::string& command, const fc::variants& arguments,
                                                               const std::string& query_string, std::string& passphrase,
                                                               bool verify = false )
            {
                auto new_arguments = arguments;
                new_arguments.push_back( fc::variant( passphrase ) );

                while( true )
                {
                    passphrase = _self->get_line( query_string + ": ", true );
                    if( passphrase.empty() ) FC_THROW_EXCEPTION(canceled_exception, "");

                    if( verify )
                    {
                        if( passphrase != _self->get_line( query_string + " (verify): ", true ) )
                        {
                            std::cout << "Passphrases do not match, try again\n";
                            continue;
                        }
                    }

                    new_arguments.back() = fc::variant( passphrase );

                    try
                    {
                        return execute_command( command, new_arguments );
                    }
                    catch( const rpc_wallet_passphrase_incorrect_exception&)
                    {
                        std::cout << "Incorrect passphrase, try again\n";
                    }
                }

                return fc::variant( false );
            }

            /** assumes last argument is passphrase */
            fc::variant execute_wallet_command_with_passphrase_query(const std::string& command, const fc::variants& arguments,
                                                                     const std::string& query_string, bool verify = false)
            {
                std::string passphrase;
                fc::variant result;
                try
                {
                    result = execute_command_with_passphrase_query( command, arguments, query_string, passphrase, verify );

                    if( command.find("wallet_create") != std::string::npos )
                        std::cout << "Created wallet \"" << _client->get_wallet()->get_filename().generic_string() << "\"\n";
                    else if ( command.find("wallet_open") != std::string::npos)
                        std::cout << "Opened wallet \"" << _client->get_wallet()->get_filename().generic_string() << "\"\n";

                    if( _client->get_wallet()->is_locked() )
                    {
                        fc::variants new_arguments { 60 * 5, passphrase }; // default to five minute timeout
                        _rpc_server->direct_invoke_method( "wallet_unlock", new_arguments );
                        std::cout << "Wallet unlocked for 5 minutes, use wallet_unlock for more time\n";
                    }
                }
                catch( const fc::canceled_exception& )
                {
                    return fc::variant( false );
                }

                return result;
            }

            fc::variant interactive_open_wallet()
            {
              if( _client->get_wallet()->is_open() ) return fc::variant( true );

              std::cout << "A wallet must be open to execute this command. You can:\n";
              std::cout << "(o) Open an existing wallet\n";
              std::cout << "(c) Create a new wallet\n";
              std::cout << "(q) Abort command\n";

              std::string choice = _self->get_line("Choose [o/c/q]: ");

              if (choice == "c")
              {
                std::string wallet_name = _self->get_line("new wallet name [default]: ");
                if (wallet_name.empty()) wallet_name = "default";

                fc::variants arguments { wallet_name };
                return execute_interactive_command( "wallet_create", arguments );
              }
              else if (choice == "o")
              {
                std::string wallet_name = _self->get_line("wallet name [default]: ");
                if (wallet_name.empty()) wallet_name = "default";

                fc::variants arguments { wallet_name };
                return execute_interactive_command( "wallet_open", arguments );
              }
              else if (choice == "q")
              {
                return fc::variant( false );
              }

              std::cout << "Wrong answer!\n";
              return fc::variant( false );
            }

            void format_and_print_result(const std::string& command, const fc::variant& result)
            {
              std::string method_name = command;
              try
              {
                // command could be alias, so get the real name of the method.
                auto method_data = _rpc_server->get_method_data(command);
                method_name = method_data.name;
              }
              catch (fc::key_not_found_exception&)
              {
                 elog( " KEY NOT FOUND " );
              }
              catch (...)
              {
                 elog( " unexpected exception " );
              }
              
              if (method_name == "help")
              {
                std::string help_string = result.as<std::string>();
                std::cout << help_string << "\n";
              }
              else if (method_name == "wallet_get_transaction_history")
              {
                auto trx_records = result.as<std::vector<wallet_transaction_record>>();
                print_transaction_history(trx_records);
              }
              else
              {
                // there was no custom handler for this particular command, see if the return type
                // is one we know how to pretty-print
                std::string result_type;
                try
                {
                  const rpc_server::method_data& method_data = _rpc_server->get_method_data(method_name);
                  result_type = method_data.return_type;

                  if (result_type == "asset")
                    std::cout << (std::string)result.as<bts::blockchain::asset>() << "\n";
                  else if (result_type == "address")
                    std::cout << (std::string)result.as<bts::blockchain::address>() << "\n";
                  else
                    std::cout << fc::json::to_pretty_string(result) << "\n";
                }
                catch (fc::key_not_found_exception&)
                {
                 elog( " KEY NOT FOUND " );
                   std::cout << "key not found \n";
                }
                catch (...)
                {
                   std::cout << "unexpected exception \n";
                }
              }
            }

            void print_transaction_history(const std::vector<wallet_transaction_record>& trx_records)
            {
                /* Print header */
                std::cout << std::setw(  3 ) << "#";
                std::cout << std::setw(  7 ) << "BLK" << ".";
                std::cout << std::setw(  5 ) << std::left << "TRX";
                std::cout << std::setw( 20 ) << "TIMESTAMP";
                std::cout << std::setw( 37 ) << "FROM";
                std::cout << std::setw( 37 ) << "TO";
                std::cout << std::setw( 16 ) << " AMOUNT";
                std::cout << std::setw(  7 ) << "FEE";
                std::cout << std::setw( 14 ) << " VOTE";
                std::cout << std::setw( 40 ) << "ID";
                std::cout << "\n----------------------------------------------------------------------------------------------";
                std::cout <<   "---------------------------------------------------------------------------------------------\n";
                std::cout << std::right;
                auto count = 1;
                char timestamp_buffer[20];
                for( auto trx_record : trx_records )
                {
                    /* Print index */
                    std::cout << std::setw( 3 ) << count;

                    /* Print block and transaction numbers */
                    std::cout << std::setw( 7 ) << trx_record.location.block_num << ".";
                    std::cout << std::setw( 5 ) << std::left << trx_record.location.trx_num;

                    /* Print timestamp */
                    auto timestamp = time_t( trx_record.received.sec_since_epoch() );
                    strftime( timestamp_buffer, std::extent<decltype( timestamp_buffer )>::value, "%F %X", localtime( &timestamp ) );
                    std::cout << std::setw( 20 ) << timestamp_buffer;

                    /* Print from address */
                    share_type withdraw_amount = 0;
                    bool sending = false;
                    {
                        std::stringstream ss;
                        bool first = true;
                        for( auto op : trx_record.trx.operations )
                        {
                            if( operation_type_enum( op.type ) == withdraw_op_type )
                            {
                                auto withdraw_op = op.as<withdraw_operation>();
                                withdraw_amount += withdraw_op.amount;

                                auto owner = _client->get_wallet()->get_owning_address( withdraw_op.balance_id );
                                if( owner.valid() ) sending |= _client->get_wallet()->is_receive_address( *owner );

                                if( first )
                                {
                                    if( owner.valid()  )
                                    {
                                        auto rec = _client->get_wallet()->get_account_record( *owner );
                                        if( rec.valid()  ) ss << rec->name;
                                        else ss << std::string( withdraw_op.balance_id );
                                    }
                                    else
                                    {
                                        ss << std::string( withdraw_op.balance_id );
                                    }
                                }
                                first = false;
                            }
                        }
                        std::cout << std::setw( 37 ) << ss.str();
                    }

                    /* Print to address */
                    share_type deposit_amount = 0;
                    name_id_type vote = 0;
                    bool receiving = false;
                    {
                        std::stringstream ss;
                        bool first = true;
                        for( auto op : trx_record.trx.operations )
                        {
                            if( operation_type_enum( op.type ) == deposit_op_type )
                            {
                                auto deposit_op = op.as<deposit_operation>();
                                deposit_amount += deposit_op.amount;
                                vote = deposit_op.condition.delegate_id;
                                if( withdraw_condition_types( deposit_op.condition.type ) == withdraw_signature_type )
                                {
                                    auto condition = deposit_op.condition.as<withdraw_with_signature>();
                                    receiving |= _client->get_wallet()->is_receive_address( condition.owner );

                                    if( first )
                                    {
                                        auto rec = _client->get_wallet()->get_account_record( condition.owner );
                                        if( rec.valid()  ) ss << rec->name;
                                        else ss << std::string( condition.owner );
                                    }
                                    first = false;
                                }
                            }
                        }
                        std::cout << std::setw( 37 ) << ss.str();
                    }

                    /* Print amount */
                    {
                        std::stringstream ss;
                        share_type amount = 0;
                        if( sending ) amount -= withdraw_amount;
                        if( receiving ) amount += deposit_amount;
                        if( amount > 0 ) ss << "+";
                        else if( amount == 0 ) ss << " ";
                        ss << amount;
                        std::cout << std::setw( 16 ) << ss.str();
                    }

                    /* Print fee */
                    std::cout << std::setw( 7 ) << (withdraw_amount - deposit_amount);

                    /* Print delegate vote */
                    {
                        std::stringstream ss;
                        auto id = (vote > 0) ? vote : name_id_type(-vote);
                        auto rec = _client->get_chain()->get_name_record(id);
                        auto name = rec.valid()  ? rec->name : "";
                        if( vote > 0 ) ss << "+";
                        else if( vote < 0 ) ss << "-";
                        else ss << " ";
                        ss << name;
                        std::cout << std::setw( 14 ) << ss.str();
                    }

                    /* Print transaction ID */
                    std::cout << std::setw( 40 ) << std::string( trx_record.trx.id() );

                    std::cout << std::right << "\n";
                    ++count;
                }
            }

#ifdef HAVE_READLINE
            char* json_command_completion_generator(const char* text, int state);
            char* json_argument_completion_generator(const char* text, int state);
#endif
      };

#ifdef HAVE_READLINE
    static cli_impl* cli_impl_instance = NULL;
    extern "C" char** json_completion_function(const char* text, int start, int end);
#endif

	  cli_impl::cli_impl( const client_ptr& client, const rpc_server_ptr& rpc_server ) :
	    _client(client),
	    _rpc_server(rpc_server)
	  {
#ifdef HAVE_READLINE
      cli_impl_instance = this;
      rl_attempted_completion_function = &json_completion_function;
#endif
    }

#ifdef HAVE_READLINE
    // implement json command completion (for function names only)
    char* cli_impl::json_command_completion_generator(const char* text, int state)
    {
      static bool first = true;
      if (first)
      {
        first = false;
        fc::variant result = execute_command("_list_json_commands", fc::variants());
        std::vector<std::string> _unsorted_json_commands = result.as<std::vector<std::string> >();
        _json_command_list.insert(_unsorted_json_commands.begin(), _unsorted_json_commands.end());
      }

      static std::set<std::string>::iterator iter;
      if (state == 0)
        iter = _json_command_list.lower_bound(text);
      else
        ++iter;
      if (iter != _json_command_list.end() &&
          iter->compare(0, strlen(text), text) == 0)
        return strdup(iter->c_str());

      rl_attempted_completion_over = 1; // suppress default filename completion
      return 0;
    }
    char* cli_impl::json_argument_completion_generator(const char* text, int state)
    {
      rl_attempted_completion_over = 1; // suppress default filename completion
      return 0;
    }
    extern "C" char* json_command_completion_generator_function(const char* text, int state)
    {
      return cli_impl_instance->json_command_completion_generator(text, state);
    }
    extern "C" char* json_argument_completion_generator_function(const char* text, int state)
    {
      return cli_impl_instance->json_argument_completion_generator(text, state);
    }
    extern "C" char** json_completion_function(const char* text, int start, int end)
    {
      if (start == 0)
        return rl_completion_matches(text, &json_command_completion_generator_function);
      else
        return rl_completion_matches(text, &json_argument_completion_generator_function);
    }

#endif

  } // end namespace detail

  cli::cli( const client_ptr& client, const rpc_server_ptr& rpc_server )
  :my( new detail::cli_impl(client, rpc_server) )
  {
    my->_self        = this;
    my->_main_thread = &fc::thread::current();

    my->_cin_complete = fc::async( [=](){ my->process_commands(); } );
  }

  cli::~cli()
  {
    try
    {
      wait();
    }
    catch ( const fc::exception& e )
    {
       wlog( "${e}", ("e",e.to_detail_string()) );
    }
  }

  void cli::wait()
  {
    my->_cin_complete.wait();
  }

  void cli::quit()
  {
    my->_cin_complete.cancel();
  }

  std::string cli::get_line( const std::string& prompt, bool no_echo )
  {
    return my->_cin_thread.async( [=](){ return my->get_line( prompt, no_echo ); } ).wait();
  }

  fc::variant cli::parse_argument_of_known_type(fc::buffered_istream& argument_stream,
                                                const rpc_server::method_data& method_data,
                                                unsigned parameter_index)
  {
    return my->parse_argument_of_known_type(argument_stream, method_data, parameter_index);
  }
  fc::variants cli::parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream,
                                                           const std::string& command)
  {
    return my->parse_unrecognized_interactive_command(argument_stream, command);
  }
  fc::variants cli::parse_recognized_interactive_command(fc::buffered_istream& argument_stream,
                                                         const rpc_server::method_data& method_data)
  {
    return my->parse_recognized_interactive_command(argument_stream, method_data);
  }
  fc::variants cli::parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command)
  {
    return my->parse_interactive_command(argument_stream, command);
  }
  fc::variant cli::execute_interactive_command(const std::string& command, const fc::variants& arguments)
  {
    return my->execute_interactive_command(command, arguments);
  }
  void cli::format_and_print_result(const std::string& command, const fc::variant& result)
  {
    return my->format_and_print_result(command, result);
  }

} } // bts::cli
