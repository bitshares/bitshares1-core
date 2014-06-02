#include <bts/cli/cli.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet.hpp>

#include <fc/io/buffered_iostream.hpp>
#include <fc/io/console.hpp>
#include <fc/io/json.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

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
//            fc::thread*                 _main_thread;
            fc::thread                  _cin_thread;
            fc::future<void>            _cin_complete;

            cli_impl( const client_ptr& client, const rpc_server_ptr& rpc_server );

            string get_prompt()const
            {
              string wallet_name =  _client->get_wallet()->get_wallet_name();
              string prompt = wallet_name;
              if( prompt == "" )
              {
                 prompt = "(wallet closed) >>> ";
              }
              else
              {
                 if( _client->get_wallet()->is_locked() )
                    prompt += " (locked) >>> ";
                 else
                    prompt += " (unlocked) >>> ";
              }
              return prompt;
            }

            void process_commands()
            { 
              try {
                 string line = _self->get_line(get_prompt());
                 while (std::cin.good())
                 {
                   string trimmed_line_to_parse(boost::algorithm::trim_copy(line));
                   if (!trimmed_line_to_parse.empty())
                   {
                     string::const_iterator iter = std::find_if(trimmed_line_to_parse.begin(), trimmed_line_to_parse.end(), ::isspace);
                     string command;
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
                     catch( const fc::key_not_found_exception& )
                     {
                       std::cout << "Error: invalid command \"" << command << "\"\n";
                     }
                     catch( const fc::canceled_exception&)
                     {
                       std::cout << "Command aborted.\n";
                     }
                     catch( const fc::exception& )
                     {
                       //std::cout << "Error parsing command \"" << command << "\": " << e.to_string() << "\n";
                       arguments = fc::variants { command };
                       auto usage = _rpc_server->direct_invoke_method("help", arguments).as_string();
                       std::cout << usage << "\n";
                     }

                     if (command_is_valid)
                     {
                       try
                       {
                         fc::variant result = _self->execute_interactive_command(command, arguments);
                         _self->format_and_print_result(command, result);
                       }
                       catch( const fc::canceled_exception& )
                       {
                         if( command == "quit" ) 
                           break;
                         std::cout << "Command aborted\n";
                       }
                       catch( const fc::exception& e )
                       {
                         std::cout << e.to_detail_string() << "\n";
                       }
                     }
                   }
                   line = _self->get_line( get_prompt()  );
                 } // while cin.good
                 _rpc_server->shutdown_rpc_server();
              } 
              catch ( const fc::exception&)
              {
                 std::cout << "\nshutting down\n";
                 _rpc_server->shutdown_rpc_server();
              }
              // user has executed "quit" or sent an EOF to the CLI to make us shut down.  
              // Tell the RPC server to close, which will allow the process to exit.
              _cin_complete.cancel();
            } 

            string get_line( const string& prompt, bool no_echo )
            {
                  string line;
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
                     {
                        FC_THROW_EXCEPTION( eof_exception, "" );
                     }
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

            fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const string& command)
            {
              try
              {
                const bts::api::method_data& method_data = _rpc_server->get_method_data(command);
                return _self->parse_recognized_interactive_command(argument_stream, method_data);
              }
              catch( const fc::key_not_found_exception& )
              {
                return _self->parse_unrecognized_interactive_command(argument_stream, command);
              }
            }

            fc::variants parse_recognized_interactive_command( fc::buffered_istream& argument_stream,
                                                               const bts::api::method_data& method_data)
            {
              fc::variants arguments;
              for (unsigned i = 0; i < method_data.parameters.size(); ++i)
              {
                try
                {
                  arguments.push_back(_self->parse_argument_of_known_type(argument_stream, method_data, i));
                }
                catch( const fc::eof_exception&)
                {
                  if (method_data.parameters[i].classification != bts::api::required_positional)
                    return arguments;
                  else //if missing required argument, prompt for that argument
                  {
                    const bts::api::parameter_data& this_parameter = method_data.parameters[i];
                    string prompt = this_parameter.name /*+ "(" + this_parameter.type  + ")"*/ + ": ";

                    //if we're prompting for a password, don't echo it to console
                    bool no_echo = (this_parameter.type == "passphrase") || (this_parameter.type == "new_passphrase");

                    string prompt_answer = _self->get_line(prompt, no_echo );
                    auto prompt_argument_stream = std::make_shared<fc::stringstream>(prompt_answer);
                    fc::buffered_istream buffered_argument_stream(prompt_argument_stream);
                    try
                    {
                      arguments.push_back(_self->parse_argument_of_known_type(buffered_argument_stream, method_data, i));
                    }
                    catch( const fc::eof_exception& e )
                    {
                        FC_THROW("Missing argument ${argument_number} of command \"${command}\"",
                                 ("argument_number", i + 1)("command", method_data.name)("cause",e.to_detail_string()) );
                    }
                    catch( fc::parse_error_exception& e )
                    {
                      FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                            ("argument_number", i + 1)("command", method_data.name)("detail", e.get_log()));
                    }
                    //if user is specifying a new password, ask him twice to be sure he typed it right
                    if (this_parameter.type == "new_passphrase")
                    {
                      std::string prompt_answer2 = _self->get_line("new_passphrase (verify): ", no_echo );
                      if (prompt_answer != prompt_answer2)
                      {
                        std::cout << "Passphrases do not match. ";
                        FC_THROW_EXCEPTION(canceled_exception,"Passphrase mismatch");
                      }
                    }
                   } //end prompting for missing required argument
                }
                catch( fc::parse_error_exception& e )
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", i + 1)("command", method_data.name)("detail", e.get_log()));
                }

                if (method_data.parameters[i].classification == bts::api::optional_named)
                  break;
              }
              return arguments;
            }

            fc::variants parse_unrecognized_interactive_command( fc::buffered_istream& argument_stream,
                                                                 const string& command)
            {
              /* Quit isn't registered with the RPC server, the RPC server spells it "stop" */
              if (command == "quit")
                return fc::variants();

              FC_THROW_EXCEPTION(key_not_found_exception, "Unknown command \"${command}\".", ("command", command));
            }

            fc::variant parse_argument_of_known_type( fc::buffered_istream& argument_stream,
                                                      const bts::api::method_data& method_data,
                                                      unsigned parameter_index)
            {
              const bts::api::parameter_data& this_parameter = method_data.parameters[parameter_index];
              if (this_parameter.type == "asset")
              {
                // for now, accept plain int, assume it's always in the base asset
                uint64_t amount_as_int;
                try
                {
                  fc::variant amount_as_variant = fc::json::from_stream(argument_stream);
                  amount_as_int = amount_as_variant.as_uint64();
                }
                catch( fc::bad_cast_exception& e )
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
                catch( fc::parse_error_exception& e )
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
                   std::cout << "ignoring eof  line: "<<__LINE__<<"\n";
                   // expected and ignored
                }
                string address_string = address_stream.str();

                try
                {
                  bts::blockchain::address::is_valid(address_string);
                }
                catch( fc::exception& e )
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
              if (command == "wallet_create")
              {
                auto wallet_name = arguments[0].as_string();
                if( fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                {
                  std::cout << "Wallet \"" << wallet_name << "\" already exists\n";
                  FC_THROW_EXCEPTION(invalid_arg_exception, "");
                }
              }
              else if (command == "wallet_import_bitcoin")
              {
                  auto filename = arguments[0].as<fc::path>();
                  if( !fc::exists( filename ) )
                  {
                     std::cout << "File \"" << filename.generic_string() << "\" not found\n";
                     FC_THROW_EXCEPTION(invalid_arg_exception, "");
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
                  return execute_wallet_command_with_passphrase_query( command, arguments, "imported wallet passphrase" );
              }
              else if (command == "wallet_export_to_json")
              {
                  auto filename = arguments[0].as<fc::path>();
                  if( fc::exists( filename ) )
                  {
                     std::cout << "File \"" << filename.generic_string() << "\" already exists\n";
                     FC_THROW_EXCEPTION(invalid_arg_exception, "");
                  }
              }
              else if (command == "wallet_create_from_json")
              {
                  auto filename = arguments[0].as<fc::path>();
                  auto wallet_name = arguments[1].as_string();
                  if( !fc::exists( filename ) )
                  {
                     std::cout << "File \"" << filename.generic_string() << "\" not found\n";
                     FC_THROW_EXCEPTION(invalid_arg_exception, "");
                  }
                  if( fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                  {
                    std::cout << "Wallet \"" << wallet_name << "\" already exists\n";
                    FC_THROW_EXCEPTION(invalid_arg_exception, "");
                  }
                  return execute_wallet_command_with_passphrase_query( command, arguments, "imported wallet passphrase" );
              }
              else if(command == "wallet_rescan_blockchain")
              {
                  if ( ! _client->get_wallet()->is_open() )
                      interactive_open_wallet();
                  if( ! _client->get_wallet()->is_unlocked() )
                  {
                      fc::variants arguments { 60 * 5 }; // default to five minute timeout
                      execute_interactive_command( "wallet_unlock", arguments );
                  } 

                  std::cout << "Rescanning blockchain...\n";
                  uint32_t start;
                  if (arguments.size() == 0)
                      start = 1;
                  else
                      start = arguments[0].as<uint32_t>();
                  while(true)
                  {
                      try {
                          std::cout << "|";
                          for(int i = 0; i < 100; i++)
                              std::cout << "-";
                          std::cout << "|\n|";
                          uint32_t next_step = 0;
                          auto cb = [start, next_step](uint32_t cur,
                                                       uint32_t last
                                                       ) mutable
                          {
                              if (start > last || cur >= last) // if WTF
                                  return;
                              if (((100*(1 + cur - start)) / (1 + last - start)) > next_step)
                              {
                                  std::cout << "=";
                                  next_step++;
                              }
                          };
                          _client->get_wallet()->scan_chain(start, -1, cb);
                          std::cout << "|\n";
                          std::cout << "Scan complete.\n";
                          return fc::variant("Scan complete.");
                      }
                      catch( const rpc_wallet_open_needed_exception& )
                      {
                          interactive_open_wallet();
                      }                
                      catch( const rpc_wallet_unlock_needed_exception& )
                      {
                          fc::variants arguments { 60 * 5 }; // default to five minute timeout
                          execute_interactive_command( "wallet_unlock", arguments );
                      }
 
                  }
              }
              else if( command == "blockchain_list_registered_accounts" )
              {
                  string start;
                  int64_t count;
                  if (arguments.size() > 0)
                      start = arguments[0].as_string();
                  else
                      start = "";
                  if (arguments.size() > 1)
                      count = arguments[1].as<int64_t>();
                  else
                      count = 50;

                  print_registered_account_list( _client->get_chain()->get_accounts(start, count) );
                  return fc::variant("Use 'blockchain_list_registered_accounts <startname> <number>' to see more.");
              }
              else if ( command == "wallet_list_receive_accounts" )
              {
                  if (! _client->get_wallet()->is_open() )
                      interactive_open_wallet();
                  auto accts = _client->get_wallet()->list_receive_accounts();
               //   std::cout << fc::json::to_pretty_string( accts ) << "\n";
                  print_receive_account_list( accts );
                  return fc::variant("OK");
              }
              else if ( command == "wallet_list_contact_accounts" )
              {
                  if (! _client->get_wallet()->is_open() )
                      interactive_open_wallet();
                  auto accts = _client->get_wallet()->list_contact_accounts();
                  print_contact_account_list( accts );
                  return fc::variant("OK");
              }
              else if(command == "quit")
              {
                FC_THROW_EXCEPTION(canceled_exception, "quit command issued");
              }
              
              return execute_command(command, arguments);
            }

            fc::variant execute_command(const string& command, const fc::variants& arguments)
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
                catch( const rpc_wallet_open_needed_exception& )
                {
                    interactive_open_wallet();
                }
                catch( const rpc_wallet_unlock_needed_exception& )
                {
                    fc::variants arguments { 60 * 5 }; // default to five minute timeout
                    execute_interactive_command( "wallet_unlock", arguments );
                }
              }
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",command) ) }

            /** assumes last argument is passphrase */
            fc::variant execute_command_with_passphrase_query( const string& command, const fc::variants& arguments,
                                                               const string& query_string, string& passphrase,
                                                               bool verify = false )
            { try {
                auto new_arguments = arguments;
                ilog( "initial args: ${new_args}", ("new_args",new_arguments) );
                new_arguments.push_back( fc::variant( passphrase ) );
                ilog( "test args: ${new_args}", ("new_args",new_arguments) );

                while( true )
                {
                    passphrase = _self->get_line( query_string + ": ", true );
                    if( passphrase.empty() ) FC_THROW_EXCEPTION(canceled_exception, "password entry aborted");

                    if( verify )
                    {
                        if( passphrase != _self->get_line( query_string + " (verify): ", true ) )
                        {
                            std::cout << "Passphrases do not match, try again\n";
                            continue;
                        }
                    }

                    new_arguments.back() = fc::variant( passphrase );
                    ilog( "passphrase: ${p}", ("p",new_arguments) );

                    try
                    {
                        return execute_command( command, new_arguments );
                    }
                    catch( const rpc_wallet_passphrase_incorrect_exception& )
                    {
                        std::cout << "Incorrect passphrase, try again\n";
                    }
                }

                return fc::variant( false );
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",command)("arguments",arguments) ) }

            /** assumes last argument is passphrase */
            fc::variant execute_wallet_command_with_passphrase_query(const string& command, const fc::variants& arguments,
                                                                     const string& query_string, bool verify = false)
            {
                string passphrase;
                auto result = execute_command_with_passphrase_query( command, arguments, query_string, passphrase, verify );

                if( _client->get_wallet()->is_locked() )
                {
                    fc::variants new_arguments { 60 * 5, passphrase }; // default to five minute timeout
                    _rpc_server->direct_invoke_method( "wallet_unlock", new_arguments );
                    std::cout << "Wallet unlocked for 5 minutes, use wallet_unlock for more time\n";
                }

                return result;
            }

            void interactive_open_wallet()
            {
              if( _client->get_wallet()->is_open() ) 
                return;

              std::cout << "A wallet must be open to execute this command. You can:\n";
              std::cout << "(o) Open an existing wallet\n";
              std::cout << "(c) Create a new wallet\n";
              std::cout << "(q) Abort command\n";

              string choice = _self->get_line("Choose [o/c/q]: ");

              if (choice == "c")
              {
                string wallet_name = _self->get_line("new wallet name [default]: ");
                if (wallet_name.empty()) 
                  wallet_name = "default";

                fc::variants arguments { wallet_name };
                try
                {
                    execute_interactive_command( "wallet_create", arguments );
                }
                catch( const fc::canceled_exception& )
                {
                }
              }
              else if (choice == "o")
              {
                string wallet_name = _self->get_line("wallet name [default]: ");
                if (wallet_name.empty()) 
                  wallet_name = "default";

                fc::variants arguments { wallet_name };
                try
                {
                    execute_interactive_command( "wallet_open", arguments );
                }
                catch( const fc::canceled_exception& )
                {
                }
              }
              else if (choice == "q")
              {
                FC_THROW_EXCEPTION(canceled_exception, "");
              }
              else
              {
                  std::cout << "Wrong answer!\n";
              }
            }

            void format_and_print_result(const string& command, const fc::variant& result)
            {
              string method_name = command;
              try
              {
                // command could be alias, so get the real name of the method.
                auto method_data = _rpc_server->get_method_data(command);
                method_name = method_data.name;
              }
              catch( const fc::key_not_found_exception& )
              {
                 elog( " KEY NOT FOUND " );
              }
              catch( ... )
              {
                 elog( " unexpected exception " );
              }
              
              if (method_name == "help")
              {
                string help_string = result.as<string>();
                std::cout << help_string << "\n";
              }
              else if (method_name == "wallet_get_transaction_history_summary")
              {
                  auto tx_history_summary = result.as<std::vector<pretty_transaction>>();
                  print_transaction_history(tx_history_summary);
              }
              else
              {
                // there was no custom handler for this particular command, see if the return type
                // is one we know how to pretty-print
                string result_type;
                try
                {
                  const bts::api::method_data& method_data = _rpc_server->get_method_data(method_name);
                  result_type = method_data.return_type;

                  if (result_type == "asset")
                    std::cout << (string)result.as<bts::blockchain::asset>() << "\n";
                  else if (result_type == "address")
                    std::cout << (string)result.as<bts::blockchain::address>() << "\n";
                  else if (result_type == "null" || result_type == "void")
                    std::cout << "OK\n";
                  else
                    std::cout << fc::json::to_pretty_string(result) << "\n";
                }
                catch( const fc::key_not_found_exception& )
                {
                   elog( " KEY NOT FOUND " );
                   std::cout << "key not found \n";
                }
                catch( ... )
                {
                   std::cout << "unexpected exception \n";
                }
              }
            }


            void print_contact_account_list(const std::vector<wallet_account_record> account_records)
            {
                std::cout << std::setw( 25 ) << std::left << "NAME";
                std::cout << std::setw( 64 ) << "KEY";
                std::cout << std::setw( 22 ) << "REGISTERED";
                std::cout << "\n";

                for( auto acct : account_records )
                {
                    if (acct.name.size() > 20)
                    {
                        std::cout << std::setw(20) << acct.name.substr(0, 20);
                        std::cout << std::setw(5) << "...";
                    }
                    else
                    {
                        std::cout << std::setw(25) << acct.name;
                    }

                    std::cout << std::setw(64) << string( acct.active_key() );

                    if (acct.registration_date == fc::time_point_sec()) {
                        std::cout << std::setw( 22 ) << "NO";
                    } else {
                        std::cout << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }
                    std::cout << "\n";
                }
            }


            void print_receive_account_list(const vector<wallet_account_record>& account_records)
            {
                std::cout << std::setw( 25 ) << std::left << "NAME";
                std::cout << std::setw( 15 ) << std::left << "BALANCE";
                std::cout << std::setw( 64 ) << "KEY";
                std::cout << std::setw( 22 ) << "REGISTERED";
                std::cout << "\n";

                for( auto acct : account_records )
                {
                    if (acct.name.size() > 20)
                    {
                        std::cout << std::setw(20) << acct.name.substr(0, 20);
                        std::cout << std::setw(5) << "...";
                    }
                    else
                    {
                        std::cout << std::setw(25) << acct.name;
                    }

                    auto balance = _client->get_wallet()->get_balance( BTS_ADDRESS_PREFIX, acct.name );
                    std::cout << std::setw(15) << string( balance );

                    std::cout << std::setw(64) << string( acct.active_key() );

                    if (acct.registration_date == fc::time_point_sec()) {
                        std::cout << std::setw( 22 ) << "NO";
                    } else {
                        std::cout << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }
                    std::cout << "\n";
                }
            }

            void print_registered_account_list(const vector<account_record> account_records )
            {
                std::cout << std::setw( 25 ) << std::left << "NAME";
                std::cout << std::setw( 64 ) << "KEY";
                std::cout << std::setw( 22 ) << "REGISTERED";
                std::cout << std::setw( 15 ) << "VOTES FOR";
                std::cout << std::setw( 15 ) << "VOTES AGAINST";
                std::cout << std::setw( 15 ) << "TRUST LEVEL";

                std::cout << "\n";
                for( auto acct : account_records )
                {
                    if (acct.name.size() > 20)
                    {
                        std::cout << std::setw(20) << acct.name.substr(0, 20);
                        std::cout << std::setw(5) << "...";
                    }
                    else
                    {
                        std::cout << std::setw(25) << acct.name;
                    }
                    std::cout << std::setw(64) << string( acct.active_key() );
                    std::cout << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                                    boost::posix_time::from_time_t( time_t( acct.registration_date.sec_since_epoch() ) ) );

                    if ( acct.is_delegate() )
                    {
                        std::cout << std::setw(15) << acct.delegate_info->votes_for;
                        std::cout << std::setw(15) << acct.delegate_info->votes_against;
                    }
                    else
                    {
                        std::cout << std::setw(15) << "N/A";
                        std::cout << std::setw(15) << "N/A";
                    }
                    if ( ! _client->get_wallet()->is_open() )
                    {
                        std::cout << "?? (wallet closed)";
                    }
                    else
                    {
                        auto trust = _client->get_wallet()->get_delegate_trust_level( acct.name );
                        std::stringstream ss;
                        if( trust != 0 )
                            ss << std::setw( 15 ) << trust;
                        else
                            ss << std::setw( 15 ) << "N/A";
                        std::cout << ss.str();
                    }

                    std::cout << "\n";
                }
            }

            void print_transaction_history(const std::vector<bts::wallet::pretty_transaction> txs)
            {
                /* Print header */
                std::cout << std::setw(  3 ) << std::right << "#";
                std::cout << std::setw(  7 ) << "BLK" << ".";
                std::cout << std::setw(  5 ) << std::left << "TRX";
                std::cout << std::setw( 20 ) << "TIMESTAMP";
                std::cout << std::setw( 37 ) << "FROM";
                std::cout << std::setw( 37 ) << "TO";
                std::cout << std::setw( 16 ) << " AMOUNT";
                std::cout << std::setw(  8 ) << " FEE";
                std::cout << std::setw( 14 ) << " VOTE";
                std::cout << std::setw( 40 ) << "ID";
                std::cout << "\n----------------------------------------------------------------------------------------------";
                std::cout <<   "----------------------------------------------------------------------------------------------\n";
                std::cout << std::right; 
                
                int count = 1;
                for( auto tx : txs )
                {
                    /* Print index */
                    std::cout << std::setw( 3 ) << count; count++;

                    /* Print block and transaction numbers */
                    std::cout << std::setw( 7 ) << tx.block_num << ".";
                    std::cout << std::setw( 5 ) << std::left << tx.trx_num;

                    /* Print timestamp */
                    std::cout << std::setw( 20 ) << boost::posix_time::to_iso_extended_string( boost::posix_time::from_time_t( tx.timestamp ) );

                    /* Print from address */
                    // TODO this only covers withdraw/deposit... what is our cli extensibility
                    // plan?
                    bool sending = false;
                    for( auto op : tx.operations )
                    {
                        if (op.get_object()["op_name"].as<string>()
                                == string("withdraw"))
                        {
                            auto withdraw_op = op.as<pretty_withdraw_op>();
                            /*
                             *  This section should be asking which account, not which address owns the balance
                             *  that is being withdrawn from.
                             *
                             *
                            auto owner = _client->get_wallet()->get_owning_address( withdraw_op.owner.first );
                            if( owner.valid() ) sending |= _client->get_wallet()->is_receive_address( *owner );
                           
                            */
                            if (false)//!withdraw_op.owner.second.empty())
                            {
                                std::cout << std::setw( 37 ) << withdraw_op.owner.second;
                            } else {
                                std::cout << std::setw( 37 ) << string(withdraw_op.owner.first);
                            }
                            break; // TODO
                        }
                    }

                    /* Print to address */
                    bool receiving = false;
                    std::pair<name_id_type, string> vote;
                    for (auto op : tx.operations) 
                    {
                        if( op.get_object()["op_name"].as<string>()
                                == string("deposit") )
                        {
                            auto deposit_op = op.as<pretty_deposit_op>();
                            receiving |= _client->get_wallet()->is_receive_address( deposit_op.owner.first );
                            vote = deposit_op.vote;
                            if (!deposit_op.owner.second.empty())
                            {
                                std::cout << std::setw(37) << deposit_op.owner.second;
                            } else {
                                std::cout << std::setw(37) << string(deposit_op.owner.first);
                            }
                            break; // TODO
                        }
                    }

                    /* Print amount */
                    {
                        std::stringstream ss;
                        share_type amount = 0;
                        //if( sending ) amount -= tx.totals_in[BTS_ADDRESS_PREFIX];
                        if( sending ) amount -= tx.totals_out[BTS_ADDRESS_PREFIX];
                        if( receiving ) amount += tx.totals_out[BTS_ADDRESS_PREFIX];
                        if( amount > 0 ) ss << "+";
                        else if( amount == 0 ) ss << " ";
                        ss << amount;
                        std::cout << std::setw( 16 ) << ss.str();
                    }

                    /* Print fee */
                    {
                        std::stringstream ss;
                        if( sending ) ss << -tx.fees[BTS_ADDRESS_PREFIX];
                        else ss << " 0";
                        std::cout << std::setw( 8 ) << ss.str();
                    }

                    /* Print delegate vote */
                    {
                        std::stringstream ss;
                        if( vote.first > 0 ) ss << "+";
                        else if( vote.first < 0 ) ss << "-";
                        else ss << " ";
                        ss << vote.second;
                        std::cout << std::setw( 14 ) << ss.str();
                    }

                    /* Print transaction ID */
                    std::cout << std::setw( 40 ) << string( tx.trx_id );

                    std::cout << std::right << "\n";
                }
            }

#ifdef HAVE_READLINE
            typedef std::map<string, bts::api::method_data> method_data_map_type;
            method_data_map_type _method_data_map;
            typedef std::map<string, string>  method_alias_map_type;
            method_alias_map_type _method_alias_map;
            method_alias_map_type::iterator _command_completion_generator_iter;
            bool _method_data_is_initialized;
            void initialize_method_data_if_necessary();
            char* json_command_completion_generator(const char* text, int state);
            char* json_argument_completion_generator(const char* text, int state);
            char** json_completion(const char* text, int start, int end);
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
      _method_data_is_initialized = false;
      rl_attempted_completion_function = &json_completion_function;
#endif
    }

#ifdef HAVE_READLINE
    void cli_impl::initialize_method_data_if_necessary()
    {
      if (!_method_data_is_initialized)
      {
        _method_data_is_initialized = true;
        std::vector<bts::api::method_data> method_data_list = _rpc_server->get_all_method_data();
        for (const bts::api::method_data& method_data : method_data_list)
        {
          _method_data_map[method_data.name] = method_data;
          _method_alias_map[method_data.name] = method_data.name;
          for (const string& alias : method_data.aliases)
            _method_alias_map[alias] = method_data.name;
        }
      }
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
      return cli_impl_instance->json_completion(text, start, end);
    }

    // implement json command completion (for function names only)
    char* cli_impl::json_command_completion_generator(const char* text, int state)
    {
      initialize_method_data_if_necessary();

      if (state == 0)
        _command_completion_generator_iter = _method_alias_map.lower_bound(text);
      else
        ++_command_completion_generator_iter;
      if (_command_completion_generator_iter != _method_alias_map.end() &&
          _command_completion_generator_iter->second.compare(0, strlen(text), text) == 0)
        return strdup(_command_completion_generator_iter->second.c_str());

      rl_attempted_completion_over = 1; // suppress default filename completion
      return 0;
    }
    char* cli_impl::json_argument_completion_generator(const char* text, int state)
    {
      rl_attempted_completion_over = 1; // suppress default filename completion
      return 0;
    }
    char** cli_impl::json_completion(const char* text, int start, int end)
    {
      if (start == 0) // beginning of line, match a command
        return rl_completion_matches(text, &json_command_completion_generator_function);
      else
      {
        // not the beginning of a line.  figure out what the type of this argument is 
        // and whether we can complete it.  First, look up the method
        string command_line_to_parse(rl_line_buffer, start);
        string trimmed_command_to_parse(boost::algorithm::trim_copy(command_line_to_parse));
        
        if (!trimmed_command_to_parse.empty())
        {
          auto alias_iter = _method_alias_map.find(trimmed_command_to_parse);
          if (alias_iter != _method_alias_map.end())
          {
            auto method_data_iter = _method_data_map.find(alias_iter->second);
            if (method_data_iter != _method_data_map.end())
            {
            }
          }
          try
          {
            const bts::api::method_data& method_data = cli_impl_instance->_rpc_server->get_method_data(trimmed_command_to_parse);
            if (method_data.name == "help")
            {
                return rl_completion_matches(text, &json_command_completion_generator_function);
            }
          }
          catch( const fc::key_not_found_exception& )
          {
            // do nothing
          }
        }
        
        return rl_completion_matches(text, &json_argument_completion_generator_function);
      }
    }

#endif

  } // end namespace detail

  cli::cli( const client_ptr& client, const rpc_server_ptr& rpc_server )
  :my( new detail::cli_impl(client, rpc_server) )
  {
    my->_self        = this;
//    my->_main_thread = &fc::thread::current();

    my->_cin_complete = fc::async( [=](){ my->process_commands(); } );
  }

  cli::~cli()
  {
    try
    {
      wait();
    }
    catch( const fc::exception& e )
    {
       wlog( "${e}", ("e",e.to_detail_string()) );
    }
  }

  void cli::wait()
  {
    my->_rpc_server->wait_on_quit();
  }

  void cli::quit()
  {
    my->_cin_complete.cancel();
  }

  string cli::get_line( const string& prompt, bool no_echo )
  {
    return my->_cin_thread.async( [=](){ return my->get_line( prompt, no_echo ); } ).wait();
  }

  fc::variant cli::parse_argument_of_known_type(fc::buffered_istream& argument_stream,
                                                const bts::api::method_data& method_data,
                                                unsigned parameter_index)
  {
    return my->parse_argument_of_known_type(argument_stream, method_data, parameter_index);
  }
  fc::variants cli::parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream,
                                                           const string& command)
  {
    return my->parse_unrecognized_interactive_command(argument_stream, command);
  }
  fc::variants cli::parse_recognized_interactive_command(fc::buffered_istream& argument_stream,
                                                         const bts::api::method_data& method_data)
  {
    return my->parse_recognized_interactive_command(argument_stream, method_data);
  }
  fc::variants cli::parse_interactive_command(fc::buffered_istream& argument_stream, const string& command)
  {
    return my->parse_interactive_command(argument_stream, command);
  }
  fc::variant cli::execute_interactive_command(const string& command, const fc::variants& arguments)
  {
    return my->execute_interactive_command(command, arguments);
  }
  void cli::format_and_print_result(const string& command, const fc::variant& result)
  {
    return my->format_and_print_result(command, result);
  }

} } // bts::cli
