#include <bts/cli/cli.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/sstream.hpp>
#include <fc/io/buffered_iostream.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/algorithm/string/trim.hpp>

#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

#include <fc/log/logger.hpp>
#include <fc/io/console.hpp>

#ifndef WIN32
# define HAVE_READLINE 1
#endif
#ifdef HAVE_READLINE
# include <readline/readline.h>
# include <readline/history.h>
#endif

namespace bts { namespace cli {

  namespace detail
  {
      class cli_impl
      {
         public:
            client_ptr                  _client;
            bts::rpc::rpc_server_ptr    _rpc_server;
            bts::cli::cli*              _self;
            fc::thread*                 _main_thread;
            fc::thread                  _cin_thread;
            fc::future<void>            _cin_complete;

            cli_impl( const client_ptr& client, const bts::rpc::rpc_server_ptr& rpc_server ) :
              _client(client),
              _rpc_server(rpc_server)
            {}

            void create_wallet_if_missing();

            std::string get_line( const std::string& prompt = ">>> ", bool no_echo = false )
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
                  return line;
            }

            void interactive_open_wallet()
            {
              if( !_client->get_wallet()->is_open() )
              {
                try
                {
                  // try to open without a passphrase first
                  _rpc_server->direct_invoke_method("open_wallet", fc::variants());
                  return;
                }
                catch (bts::rpc::rpc_wallet_passphrase_incorrect_exception&)
                {
                }
                catch (const fc::exception& e)
                {
                }
                catch (...)
                {
                }

                while (1)
                {
                  std::string passphrase = _self->get_line("wallet passphrase: ", true);
                  if (passphrase.empty())
                    FC_THROW_EXCEPTION(canceled_exception, "User gave up entering wallet passphrase");
                  try
                  {
                    fc::variants arguments{_client->get_wallet()->get_current_user(), passphrase};
                    _rpc_server->direct_invoke_method("open_wallet", arguments);
                    return;
                  }
                  catch (bts::rpc::rpc_wallet_passphrase_incorrect_exception&)
                  {
                    std::cout << "Invalid passphrase, try again\n";
                  }
                }
              }
            }

            void interactive_unlock_wallet()
            {
              if( _client->get_wallet()->is_locked() )
              {
                while (1)
                {
                  std::string passphrase = _self->get_line("spending passphrase: ", true);
                  if (passphrase.empty())
                    FC_THROW_EXCEPTION(canceled_exception, "User gave up entering spending passphrase");
                  try
                  {
                    fc::variants arguments{passphrase, 60 * 5}; // default to five minute timeout
                    _rpc_server->direct_invoke_method("walletpassphrase", arguments);
                    return;
                  }
                  catch (bts::rpc::rpc_wallet_passphrase_incorrect_exception&)
                  {
                    std::cout << "Invalid passphrase, try again\n";
                  }
                }
              }
            }

            fc::variant interactive_import_wallet(const fc::variants& arguments)
            {
                try
                {
                  std::pair<fc::path, std::string> argument_pair(arguments[0].as<fc::path>(), "");
                  fc::variants new_arguments;
                  new_arguments.push_back(fc::variant(argument_pair));
                  return execute_command_and_prompt_for_passphrases("import_wallet", new_arguments);
                }
                catch (...)
                {
                }

                while (1)
                {
                  std::string passphrase = _self->get_line("imported wallet passphrase: ", true);
                  if (passphrase.empty())
                      return fc::variant(false);

                  try
                  {
                    std::pair<fc::path, std::string> argument_pair(arguments[0].as<fc::path>(), passphrase);
                    fc::variants new_arguments;
                    new_arguments.push_back(fc::variant(argument_pair));
                    return execute_command_and_prompt_for_passphrases("import_wallet", new_arguments);
                  }
                  catch (...)
                  {
                    std::cout << "Invalid passphrase, try again\n";
                  }
                }
            }

            fc::variant execute_command_and_prompt_for_passphrases(const std::string& command, const fc::variants& arguments)
            {
              for (;;)
              {
                // try to execute the method.  If the method needs the wallet to be
                // unlocked, it will throw an exception, and we'll attempt to
                // unlock it and then retry the command
                try
                {
                  return _rpc_server->direct_invoke_method(command, arguments);
                }
                catch (bts::rpc::rpc_wallet_open_needed_exception&)
                {
                  try
                  {
                    interactive_open_wallet();
                  }
                  catch (fc::canceled_exception&)
                  {
                    std::cout << "Failed to open wallet, aborting \"" << command << "\" command\n";
                    return fc::variant(false);
                  }
                }
                catch (bts::rpc::rpc_wallet_unlock_needed_exception&)
                {
                  try
                  {
                    interactive_unlock_wallet();
                  }
                  catch (fc::canceled_exception&)
                  {
                    std::cout << "Failed to unlock wallet, aborting \"" << command << "\" command\n";
                    return fc::variant(false);
                  }
                }
              }
            }

            fc::variant interactive_sendtoaddress(const std::string& command, const fc::variants& arguments)
            {
              // validate arguments
              FC_ASSERT(arguments.size() >= 2 && arguments.size() <= 4);

              // _create_sendtoaddress_transaction takes the same arguments as sendtoaddress
              fc::variant result = execute_command_and_prompt_for_passphrases("_create_sendtoaddress_transaction", arguments);
              bts::blockchain::signed_transaction transaction = result.as<bts::blockchain::signed_transaction>();

              while (1)
              {
                  std::string response;
                  std::cout << "About to broadcast transaction:\n\n";
                  std::cout << _client->get_wallet()->get_transaction_info_string(*_client->get_chain(), transaction) << "\n";
                  std::cout << "Broadcast this transaction? (yes/no)\n";
                  std::cin >> response;

                  if (response == "yes")
                  {
                    result = execute_command_and_prompt_for_passphrases("_send_transaction", fc::variants{fc::variant(transaction)});
                    bts::blockchain::transaction_id_type transaction_id = result.as<bts::blockchain::transaction_id_type>();
                    std::cout << "Transaction broadcasted (" << (std::string)transaction_id << ")\n";
                    return result;
                  }
                  else if (response == "no")
                  {
                    std::cout << "Transaction canceled\n";
                    break;
                  }
              }

              return fc::variant(false);
            }

            fc::variant interactive_rescan(const std::string& command, const fc::variants& arguments)
            {
              // implement the "rescan" function locally instead of going through JSON,
              // which will let us display progress messages
              FC_ASSERT(arguments.size() == 0 || arguments.size() == 1);
              uint32_t block_num = 0;
              if (arguments.size() == 1)
                block_num = (uint32_t)arguments[0].as_uint64();

              _client->get_wallet()->scan_chain( *_client->get_chain(), block_num, [](uint32_t cur, uint32_t last, uint32_t trx, uint32_t last_trx)
                {
                    std::cout << "Scanning transaction " <<  cur << "." << trx <<"  of " << last << "." << last_trx << "         \r";
                });
              return fc::variant();
            }

            fc::variant parse_argument_of_known_type( fc::buffered_istream& argument_stream,
                                                      const bts::rpc::rpc_server::method_data& method_data,
                                                      unsigned parameter_index)
            {
              const bts::rpc::rpc_server::parameter_data& this_parameter = method_data.parameters[parameter_index];
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

            fc::variants parse_unrecognized_interactive_command( fc::buffered_istream& argument_stream,
                                                                 const std::string& command)
            {
              /* Quit isn't registered with the RPC server, the RPC server spells it "stop" */
              if (command == "quit")
                return fc::variants();

              FC_THROW_EXCEPTION(key_not_found_exception, "Unknown command \"${command}\".", ("command", command));
            }

            fc::variants parse_recognized_interactive_command( fc::buffered_istream& argument_stream,
                                                               const bts::rpc::rpc_server::method_data& method_data)
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
                  if (!method_data.parameters[i].required)
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
              }
              return arguments;
            }

            fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command)
            {
              try
              {
                const bts::rpc::rpc_server::method_data& method_data = _rpc_server->get_method_data(command);
                return _self->parse_recognized_interactive_command(argument_stream, method_data);
              }
              catch (fc::key_not_found_exception&)
              {
                return _self->parse_unrecognized_interactive_command(argument_stream, command);
              }
            }

            fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments)
            {
              if (command == "sendtoaddress")
              {
                // the raw json version sends immediately, in the interactive version we want to
                // generate the transaction, have the user confirm it, then broadcast it
                interactive_sendtoaddress(command, arguments);
                return fc::variant();
              }
              else if( command == "listdelegates" )
              {
                if( arguments.size() == 2 )
                  _self->list_delegates( (uint32_t)arguments[1].as<int64_t>() );
                else
                  _self->list_delegates( );
                return fc::variant();
              }
              else if (command == "rescan")
              {
                return interactive_rescan(command, arguments);
              }
              else if (command == "import_wallet")
              {
                return interactive_import_wallet(arguments);
              }
              else if(command == "quit")
              {
                FC_THROW_EXCEPTION(canceled_exception, "quit command issued");
              }
              else
              {
                return execute_command_and_prompt_for_passphrases(command, arguments);
              }
              return fc::variant();
            }

            void format_and_print_result(const std::string& command, const fc::variant& result)
            {
              if (command == "sendtoaddress")
              {
                // sendtoaddress does its own output formatting
              }
              else if(command == "list_receive_addresses")
              {
                std::cout << std::setw( 33 ) << std::left << "address" << " : " << "account" << "\n";
                std::cout << "--------------------------------------------------------------------------------\n";
                auto addrs = _client->get_wallet()->get_receive_addresses();
                for( auto addr : addrs )
                  std::cout << std::setw( 33 ) << std::left << std::string(addr) << " : " << addr.memo << "\n";
                std::cout << std::right;
              }
              else if (command == "help")
              {
                std::string help_string = result.as<std::string>();
                std::cout << help_string << "\n";
              }
              else if (command == "rescan")
              {
                std::cout << "\ndone scanning block chain\n";
              }
              else if (command == "get_transaction_history")
              {
                auto trx_history = result.as<std::vector<transaction_state>>();
                print_transaction_history(trx_history);
              }
              else
              {
                // there was no custom handler for this particular command, see if the return type
                // is one we know how to pretty-print
                std::string result_type;
                try
                {
                  const bts::rpc::rpc_server::method_data& method_data = _rpc_server->get_method_data(command);
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
                }
                catch (...)
                {
                }
              }
            }

            void print_transaction_history(std::vector<transaction_state>& trx_states)
            {
                /* Print header */
                std::cout << std::setw(  4 ) << "#";
                std::cout << std::setw(  6 ) << "BLK" << ".";
                std::cout << std::setw(  5 ) << std::left << "TRX";
                std::cout << std::setw( 21 ) << "CONFIRMED";
                std::cout << std::setw( 13 ) << " AMOUNT";
                std::cout << std::setw( 40 ) << "FROM";
                std::cout << std::setw( 40 ) << "TO";
                std::cout << std::setw(  6 ) << "FEE";
                std::cout << std::setw( 14 ) << " VOTE";
                std::cout << std::setw( 40 ) << "ID";
                std::cout << "\n-----------------------------------------------------------------------------------------------";
                std::cout <<   "-----------------------------------------------------------------------------------------------\n";
                std::cout << std::right;

                auto count = 1;
                char timestamp_buffer[20];
                for( auto trx_state : trx_states )
                {
                    /* Print index */
                    std::cout << std::setw( 4 ) << count;

                    /* Print block and transaction numbers */
                    std::cout << std::setw( 6 ) << trx_state.block_num << ".";
                    std::cout << std::setw( 5 ) << std::left << trx_state.trx_num;

                    /* Print timestamp */
                    auto timestamp = time_t(trx_state.confirm_time.sec_since_epoch());
                    strftime(timestamp_buffer, std::extent<decltype(timestamp_buffer)>::value, "%F %X", localtime(&timestamp));
                    std::cout << std::setw( 21 ) << timestamp_buffer;

                    /* Print amount */
                    {
                        std::stringstream ss;
                        if (trx_state.delta_balance[0] > 0) ss << "+" << trx_state.delta_balance[0];
                        else if (trx_state.delta_balance[0] < 0) ss << "-" << -trx_state.delta_balance[0];
                        else ss << " 0";
                        std::cout << std::setw( 13 ) << ss.str();
                    }

                    /* Print from addresses */
                    {
                        std::stringstream ss;
                        auto n = 0;
                        for (auto pair : trx_state.from)
                        {
                            if (n) ss << ", ";
                            ss << pair.second;
                            ++n;
                            break; // TODO
                        }
                        std::cout << std::setw( 40 ) << ss.str();
                    }

                    /* Print to addresses */
                    {
                        std::stringstream ss;
                        auto n = 0;
                        for (auto pair : trx_state.to)
                        {
                            if (n) ss << ", ";
                            ss << pair.second;
                            ++n;
                            break; // TODO
                        }
                        std::cout << std::setw( 40 ) << ss.str();
                    }

                    /* Print fee */
                    std::cout << std::setw( 6 ) << trx_state.fees[0];

                    /* Print delegate vote */
                    {
                        std::stringstream ss;
                        if (trx_state.trx.vote > 0) ss << "+" << _rpc_server->get_client()->get_chain()->lookup_delegate(trx_state.trx.vote)->name;
                        else ss << "-" << _rpc_server->get_client()->get_chain()->lookup_delegate(-trx_state.trx.vote)->name;
                        std::cout << std::setw( 14 ) << ss.str();
                    }

                    /* Print transaction ID */
                    std::cout << std::setw( 40 ) << std::string( trx_state.trx.id() );

                    std::cout << std::right << "\n";
                    ++count;
                }
            }

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

            // Parse the given line into a command and arguments, returning them in the form our
            // json-rpc server functions require them.
            void parse_json_command(const std::string& line_to_parse, std::string& command, fc::variants& arguments)
            {
              // the first whitespace-separated token on the line should be an un-quoted
              // JSON-RPC command name
              command.clear();
              arguments.clear();
              std::string::const_iterator iter = std::find_if(line_to_parse.begin(), line_to_parse.end(), ::isspace);
              if (iter != line_to_parse.end())
              {
                // then there are arguments to this function
                size_t first_space_pos = iter - line_to_parse.begin();
                command = line_to_parse.substr(0, first_space_pos);
                fc::istream_ptr argument_stream = std::make_shared<fc::stringstream>((line_to_parse.substr(first_space_pos + 1)));
                fc::buffered_istream buffered_argument_stream(argument_stream);
                while (1)
                {
                  try
                  {
                    arguments.emplace_back(fc::json::from_stream(buffered_argument_stream));
                  }
                  catch (fc::eof_exception&)
                  {
                    break;
                  }
                  catch (fc::parse_error_exception& e)
                  {
                    FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                         ("argument_number", arguments.size() + 1)("command", command)("detail", e.get_log()));
                  }
                }
              }
              else
              {
                // no arguments
                command = line_to_parse;
              }
            }
      };

    void cli_impl::create_wallet_if_missing()
    {
      std::string user = "default";
      auto wallet_dat = _client->get_wallet()->get_wallet_filename_for_user(user);
      if( !fc::exists( wallet_dat ) )
      {
        std::cout << "Creating wallet \""<< user << "\"\n";
        std::cout << "You will be asked to provide two passphrase, the first passphrase\n";
        std::cout << "encrypts the entire contents of your wallet on disk.  The second\n";
        std::cout << "passphrase will only encrypt your private keys.\n\n";

        std::cout << "Please set a passphrase for encrypting your wallet: \n";
        std::string pass1, pass2;
        pass1  = _self->get_line("wallet passphrase: ", true);
        while( pass1 != pass2 )
        {
          pass2 = _self->get_line("wallet passphrase (again): ", true);
          if( pass2 != pass1 )
          {
            std::cout << "Your passphrases did not match, please try again.\n";
            pass2 = std::string();
            pass1 = _self->get_line("wallet passphrase: ", true);
          }
        }
        if( pass1 == std::string() )
        {
          std::cout << "No passphrase provided, your wallet will be stored unencrypted.\n";
        }

        std::cout << "\nPlease set a passphrase for encrypting your private keys: \n";
        std::string keypass1, keypass2;
        bool retry = false;
        keypass1  = _self->get_line("spending passphrase: ", true);
        while( keypass1 != keypass2 )
        {
          if( keypass1.size() > 8 )
              keypass2 = _self->get_line("spending passphrase (again): ", true);
          else
          {
              std::cout << "Your key passphrase must be more than 8 characters.\n";
              retry = true;
          }
          if( keypass2 != keypass1 )
          {
              std::cout << "Your passphrase did not match.\n";
              retry = true;
          }
          if( retry )
          {
            retry = false;
            std::cout << "Please try again.\n";
            keypass2 = std::string();
            keypass1 = _self->get_line("spending passphrase: ", true);
          }
        }
        if( keypass1 == std::string() )
        {
          std::cout << "No passphrase provided, your wallet will be stored unencrypted.\n";
        }

        _client->get_wallet()->create( user, pass1, keypass1 );
        std::cout << "Wallet created.\n";
      }

    }
  } // end namespace detail

  cli::cli( const client_ptr& client, const bts::rpc::rpc_server_ptr& rpc_server )
  :my( new detail::cli_impl(client, rpc_server) )
  {
    my->_self        = this;
    my->_main_thread = &fc::thread::current();

    my->create_wallet_if_missing();
    my->interactive_open_wallet();

    my->_cin_complete = fc::async( [=](){ my->process_commands(); } );
  }

   cli::~cli()
   {
      try
      {
        my->_client->get_wallet()->save();
        wait();
      }
      catch ( const fc::exception& e )
      {
         wlog( "${e}", ("e",e.to_detail_string()) );
      }
   }

   void cli::list_delegates( uint32_t count )
   {
        auto delegates = my->_client->get_chain()->get_delegates( count );

        std::cerr<<"Delegate Ranking\n";
        std::cerr<<std::setw(6)<<"Rank "<<"  "
                 <<std::setw(6)<<"ID"<<"  "
                 <<std::setw(18)<<"NAME"<<"  "
                 <<std::setw(18)<<"VOTES FOR"<<"  "
                 <<std::setw(18)<<"VOTES AGAINST"<<"  "
                 <<"    PERCENT\n";
        std::cerr<<"==========================================================================================\n";
        for( uint32_t i = 0; i < delegates.size(); ++i )
        {
           std::cerr << std::setw(6)  << i               << "  "
                     << std::setw(6)  << delegates[i].delegate_id   << "  "
                     << std::setw(18) << delegates[i].name          << "  "
                     << std::setw(18) << delegates[i].votes_for     << "  "
                     << std::setw(18) << delegates[i].votes_against << "  "
                     << std::setw(18) << double((delegates[i].total_votes()*10000)/BTS_BLOCKCHAIN_BIP)/100  << "%   |\n";
           ++i;
           if( i == count ) break;
        }

      // client()->get_chain()->dump_delegates( count );
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
                                                const bts::rpc::rpc_server::method_data& method_data,
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
                                                         const bts::rpc::rpc_server::method_data& method_data)
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
