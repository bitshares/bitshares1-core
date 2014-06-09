#include <bts/cli/cli.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/rpc/exceptions.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/withdraw_types.hpp>

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

#include <boost/optional.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>


#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

//#undef HAVE_READLINE
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

            bool                        _quit;
            bool                        show_raw_output;

            std::ostream*                  _out;   //cout | log_stream | tee(cout,log_stream) | null_stream
            std::istream*                  _input_stream;
            boost::optional<std::ostream&> _input_log_stream;

            cli_impl(const client_ptr& client, std::istream* input_stream, std::ostream* output_stream);

            string get_prompt()const
            {
              string wallet_name =  _client->get_wallet()->get_wallet_name();
              string prompt = wallet_name;
              if( prompt == "" )
              {
                 prompt = "(wallet closed) " CLI_PROMPT_SUFFIX;
              }
              else
              {
                 if( _client->get_wallet()->is_locked() )
                    prompt += " (locked) " CLI_PROMPT_SUFFIX;
                 else
                    prompt += " (unlocked) " CLI_PROMPT_SUFFIX;
              }
              return prompt;
            }

            void parse_and_execute_interactive_command(string command, 
                                                       fc::istream_ptr argument_stream )
            { 
              if (command == "enable_raw")
              {
                  show_raw_output = true;
                  return;
              }
              else if (command == "disable_raw")
              {
                  show_raw_output = false;
                  return;
              }

              fc::buffered_istream buffered_argument_stream(argument_stream);

              bool command_is_valid = false;
              fc::variants arguments;
              try
              {
                arguments = _self->parse_interactive_command(buffered_argument_stream, command);
                ilog( "command: ${c} ${a}", ("c",command)("a",arguments) ); 
                command_is_valid = true;
              }
              catch( const rpc::unknown_method& )
              {
                 if( command.size() )
                   if( _out ) (*_out) << "Error: invalid command \"" << command << "\"\n";
              }
              catch( const fc::canceled_exception&)
              {
                if( _out ) (*_out) << "Command aborted.\n";
              }
              catch( const fc::exception& e)
              {
                if( _out ) (*_out) << e.to_detail_string() <<"\n";
                if( _out ) (*_out) << "Error parsing command \"" << command << "\": " << e.to_string() << "\n";
                arguments = fc::variants { command };
                auto usage = _rpc_server->direct_invoke_method("help", arguments).as_string();
                if( _out ) (*_out) << usage << "\n";
              }

              //if command is valid, go ahead and execute it
              if (command_is_valid)
              {
                try
                {
                  fc::variant result = _self->execute_interactive_command(command, arguments);
                  _self->format_and_print_result(command, arguments, result);
                }
                catch( const fc::canceled_exception&)
                {
                  throw;
                }
                catch( const fc::exception& e )
                {
                  if( _out ) (*_out) << e.to_detail_string() << "\n";
                }
              }
            } //parse_and_execute_interactive_command

            bool execute_command_line(const string& line)
            { try {
              ilog( "${c}", ("c",line) );
              string trimmed_line_to_parse(boost::algorithm::trim_copy(line));
              /** 
               *  On some OS X systems, std::stringstream gets corrupted and does not throw eof
               *  when expected while parsing the command.  Adding EOF (0x04) characater at the
               *  end of the string casues the JSON parser to recognize the EOF rather than relying
               *  on stringstream.  
               *
               *  @todo figure out how to fix things on these OS X systems.
               */
              trimmed_line_to_parse += string(" ") + char(0x04);
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
                try
                {
                  parse_and_execute_interactive_command(command,argument_stream);
                }
                catch( const fc::canceled_exception& )
                {
                  if( command == "quit" ) 
                    return false;
                  if( _out ) (*_out) << "Command aborted\n";
                }
              } //end if command line not empty
              return true;
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",line) ) }

            void process_commands()
            { 
              try {
                  FC_ASSERT( _input_stream != nullptr );
                 string line = get_line(get_prompt());
                 while (_input_stream->good() && !_quit )
                 {
                   if (!execute_command_line(line))
                     break;
                   if( !_quit )
                      line = get_line( get_prompt() );
                 } // while cin.good
                 _rpc_server->shutdown_rpc_server();
              } 
              catch ( const fc::exception&)
              {
                 if( _out ) (*_out) << "\nshutting down\n";
                 _rpc_server->shutdown_rpc_server();
              }
              // user has executed "quit" or sent an EOF to the CLI to make us shut down.  
              // Tell the RPC server to close, which will allow the process to exit.
              _cin_complete.cancel();
            } 

            string get_line( const string& prompt = CLI_PROMPT_SUFFIX, bool no_echo = false)
            {
              return _cin_thread.async( [=](){ return get_line_internal( prompt, no_echo ); } ).wait();
            }

            string get_line_internal( const string& prompt, bool no_echo )
            {
                  if( _quit ) return std::string();
                  FC_ASSERT( _input_stream != nullptr );

                  //FC_ASSERT( _self->is_interactive() );
                  string line;
                  if ( no_echo )
                  {
                      if( _out ) (*_out) << prompt;
                      // there is no need to add input to history when echo is off, so both Windows and Unix implementations are same
                      fc::set_console_echo(false);
                      std::getline( *_input_stream, line );
                      fc::set_console_echo(true);
                      if( _out ) (*_out) << std::endl;
                  }
                  else
                  {
                  #ifdef HAVE_READLINE 
                     char* line_read = nullptr;
                     _out->flush(); //readline doesn't use cin, so we must manually flush _out
                     line_read = readline(prompt.c_str());
                     if(line_read && *line_read)
                         add_history(line_read);
                     if( line_read == nullptr )
                     {
                        FC_THROW_EXCEPTION( fc::eof_exception, "" );
                     }
                     line = line_read;
                     free(line_read);
                  #else
                      if( _out ) (*_out) <<prompt;
                     std::getline( *_input_stream, line );
                  #endif
                  if (_input_log_stream)
                    *_input_log_stream << line << std::endl;
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
                    bool passphrase_type = (this_parameter.type == "passphrase") || (this_parameter.type == "new_passphrase");
                    bool no_echo = passphrase_type;
                    string prompt_answer = get_line(prompt, no_echo );
                    if (passphrase_type)
                    {
                      //if user is specifying a new password, ask him twice to be sure he typed it right
                      if (this_parameter.type == "new_passphrase")
                      {
                        string prompt_answer2 = get_line("new_passphrase (verify): ", no_echo );
                        if (prompt_answer != prompt_answer2)
                        {
                          if( _out ) (*_out) << "Passphrases do not match. ";
                          FC_THROW_EXCEPTION(fc::canceled_exception,"Passphrase mismatch");
                        }
                      }
                      arguments.push_back(fc::variant(prompt_answer));
                    }
                    else //not a passphrase
                    {
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
                    } //else not a passphrase
                 } //end prompting for missing required argument
                } //end catch eof_exception
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

              FC_THROW_EXCEPTION(fc::key_not_found_exception, "Unknown command \"${command}\".", ("command", command));
            }

            fc::variant parse_argument_of_known_type( fc::buffered_istream& argument_stream,
                                                      const bts::api::method_data& method_data,
                                                      unsigned parameter_index)
            { try {
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
                   // if( _out ) (*_out) << "ignoring eof  line: "<<__LINE__<<"\n";
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
                  auto tmp = fc::json::from_stream( argument_stream );
                  return  tmp;
                }
                catch( fc::parse_error_exception& e )
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
              }
            } FC_RETHROW_EXCEPTIONS( warn, "", ("parameter_index",parameter_index) ) }

            fc::variant execute_interactive_command(const string& command, const fc::variants& arguments)
            {
              if (command == "wallet_import_bitcoin")
              {
                  auto filename = arguments[0].as<fc::path>();
                  if( !fc::exists( filename ) )
                  {
                     if( _out ) (*_out) << "File \"" << filename.generic_string() << "\" not found\n";
                     FC_THROW_EXCEPTION(fc::invalid_arg_exception, "");
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
                     if( _out ) (*_out) << "File \"" << filename.generic_string() << "\" already exists\n";
                     FC_THROW_EXCEPTION(fc::invalid_arg_exception, "");
                  }
              }
              else if (command == "wallet_create_from_json")
              {
                  auto filename = arguments[0].as<fc::path>();
                  auto wallet_name = arguments[1].as_string();
                  if( !fc::exists( filename ) )
                  {
                     if( _out ) (*_out) << "File \"" << filename.generic_string() << "\" not found\n";
                     FC_THROW_EXCEPTION(fc::invalid_arg_exception, "");
                  }
                  if( fc::exists( _client->get_wallet()->get_data_directory() / wallet_name ) )
                  {
                    if( _out ) (*_out) << "Wallet \"" << wallet_name << "\" already exists\n";
                    FC_THROW_EXCEPTION(fc::invalid_arg_exception, "");
                  }
                  return execute_wallet_command_with_passphrase_query( command, arguments, "imported wallet passphrase" );
              }
              else if(command == "wallet_rescan_blockchain")
              {
                  if ( ! _client->get_wallet()->is_open() )
                      interactive_open_wallet();
                  if( ! _client->get_wallet()->is_unlocked() )
                  {
                    // unlock wallet for 5 minutes
                    fc::istream_ptr argument_stream = std::make_shared<fc::stringstream>("300");
                    try
                    {
                      parse_and_execute_interactive_command( "wallet_unlock", argument_stream );
                    }
                    catch( const fc::canceled_exception& )
                    {
                    }
                  } 

                  if( _out ) (*_out) << "Rescanning blockchain...\n";
                  uint32_t start;
                  if (arguments.size() == 0)
                      start = 1;
                  else
                      start = arguments[0].as<uint32_t>();
                  while(true)
                  {
                      try {
                          if( _out ) (*_out) << "|";
                          for(int i = 0; i < 100; i++)
                              if( _out ) (*_out) << "-";
                          if( _out ) (*_out) << "|\n|=";
                          uint32_t next_step = 0;
                          auto cb = [=](uint32_t cur,
                                                       uint32_t last
                                                       ) mutable
                          {
                              if (start > last || cur >= last) // if WTF
                                  return;
                              if (((100*(1 + cur - start)) / (1 + last - start)) > next_step)
                              {
                                  if( _out ) (*_out) << "=";
                                  next_step++;
                              }
                          };
                          _client->get_wallet()->scan_chain(start, -1, cb);
                          if( _out ) (*_out) << "|\n";
                          if( _out ) (*_out) << "Scan complete.\n";
                          return fc::variant("Scan complete.");
                      }
                      catch( const rpc_wallet_open_needed_exception& )
                      {
                          interactive_open_wallet();
                      }                
                      catch( const rpc_wallet_unlock_needed_exception& )
                      {
                        // unlock wallet for 5 minutes
                        fc::istream_ptr argument_stream = std::make_shared<fc::stringstream>("300");
                        try
                        {
                          parse_and_execute_interactive_command( "wallet_unlock", argument_stream );
                        }
                        catch( const fc::canceled_exception& )
                        {
                        }
                      }
 
                  }
              }
              else if ( command == "wallet_list_receive_accounts" )
              {
                  if (! _client->get_wallet()->is_open() )
                      interactive_open_wallet();
                  auto accts = _client->get_wallet()->list_receive_accounts();
               //   if( _out ) (*_out) << fc::json::to_pretty_string( accts ) << "\n";
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
              else if(command == "quit" || command == "stop" || command == "exit")
              {
                _quit = true;
                FC_THROW_EXCEPTION(fc::canceled_exception, "quit command issued");
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
                    fc::istream_ptr unlock_time_arguments = std::make_shared<fc::stringstream>("300"); // default to five minute timeout
                    parse_and_execute_interactive_command( "wallet_unlock", unlock_time_arguments );
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
                    passphrase = get_line( query_string + ": ", true );
                    if( passphrase.empty() ) FC_THROW_EXCEPTION(fc::canceled_exception, "password entry aborted");

                    if( verify )
                    {
                        if( passphrase != get_line( query_string + " (verify): ", true ) )
                        {
                            if( _out ) (*_out) << "Passphrases do not match, try again\n";
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
                        if( _out ) (*_out) << "Incorrect passphrase, try again\n";
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
                    if( _out ) (*_out) << "Wallet unlocked for 5 minutes, use wallet_unlock for more time\n";
                }

                return result;
            }

            void interactive_open_wallet()
            {
              if( _client->get_wallet()->is_open() ) 
                return;

              if( _out ) (*_out) << "A wallet must be open to execute this command. You can:\n";
              if( _out ) (*_out) << "(o) Open an existing wallet\n";
              if( _out ) (*_out) << "(c) Create a new wallet\n";
              if( _out ) (*_out) << "(q) Abort command\n";

              string choice = get_line("Choose [o/c/q]: ");

              if (choice == "c")
              {
                fc::istream_ptr argument_stream = std::make_shared<fc::stringstream>();
                try
                {
                  parse_and_execute_interactive_command( "wallet_create", argument_stream );
                }
                catch( const fc::canceled_exception& )
                {
                }
              }
              else if (choice == "o")
              {
                fc::istream_ptr argument_stream = std::make_shared<fc::stringstream>();
                try
                {
                  parse_and_execute_interactive_command( "wallet_open", argument_stream );
                }
                catch( const fc::canceled_exception& )
                {
                }
              }
              else if (choice == "q")
              {
                FC_THROW_EXCEPTION(fc::canceled_exception, "");
              }
              else
              {
                  if( _out ) (*_out) << "Wrong answer!\n";
              }
            }

            void format_and_print_result(const string& command,
                                         const fc::variants arguments,
                                         const fc::variant& result)
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

              if (show_raw_output)
              {
                  string result_type;
                  const bts::api::method_data& method_data = _rpc_server->get_method_data(method_name);
                  result_type = method_data.return_type;

                  if (result_type == "asset")
                  {
                    if( _out ) (*_out) << (string)result.as<bts::blockchain::asset>() << "\n";
                  }
                  else if (result_type == "address")
                  {
                    if( _out ) (*_out) << (string)result.as<bts::blockchain::address>() << "\n";
                  }
                  else if (result_type == "null" || result_type == "void")
                  {
                    if( _out ) (*_out) << "OK\n";
                  }
                  else
                  {
                    if( _out ) (*_out) << fc::json::to_pretty_string(result) << "\n";
                  }
              }
             
              if (method_name == "help")
              {
                string help_string = result.as<string>();
                if( _out ) (*_out) << help_string << "\n";
              }
              else if (method_name == "wallet_account_transaction_history")
              {
                  auto tx_history_summary = result.as<std::vector<pretty_transaction>>();
                  print_transaction_history(tx_history_summary);
              }
              else if (method_name == "wallet_account_balance" )
              {
                 auto bc = _client->get_chain();
                  auto summary = result.as<unordered_map<string, map<string, share_type>>>();
                  for( auto accts : summary )
                  {
                      if( _out ) (*_out) << accts.first << ":\n";
                      for( auto balance : accts.second )
                      {
                         if( _out ) (*_out) << "    " << bc->to_pretty_asset( asset( balance.second, bc->get_asset_id( balance.first) ) ) <<"\n";//fc::to_pretty_string(balance.second) << " " << balance.first <<"\n"; 
                      }
                  }
              }
              else if (method_name == "wallet_list")
              {
                  auto wallets = result.as<vector<string>>();
                  for (auto wallet : wallets)
                      if( _out ) (*_out) << wallet << "\n";
              }
              else if (method_name == "wallet_list_unspent_balances" )
              {
                  auto balance_recs = result.as<vector<wallet_balance_record>>();
                  if( _out ) (*_out) << std::right;
                  if( _out ) (*_out) << std::setw(18) << "BALANCE";
                  if( _out ) (*_out) << std::right << std::setw(40) << "OWNER";
                  if( _out ) (*_out) << std::right << std::setw(25) << "VOTE";
                  if( _out ) (*_out) << "\n";
                  if( _out ) (*_out) << "-------------------------------------------------------------";
                  if( _out ) (*_out) << "-------------------------------------------------------------\n";
                  for( auto balance_rec : balance_recs )
                  {
                      if( _out ) (*_out) << std::setw(18) << balance_rec.balance;
                      switch (withdraw_condition_types(balance_rec.condition.type))
                      {
                          case (withdraw_signature_type):
                          {
                              auto cond = balance_rec.condition.as<withdraw_with_signature>();
                              auto acct_rec = _client->get_wallet()->get_account_record( cond.owner );
                              string owner;
                              if ( acct_rec.valid() )
                                  owner = acct_rec->name;
                              else
                                  owner = string( balance_rec.owner() );

                              if (owner.size() > 36)
                              {
                                  if( _out ) (*_out) << std::setw(40) << owner.substr(0, 31) << "...";
                              }
                              else
                              {
                                  if( _out ) (*_out) << std::setw(40) << owner;
                              }

                              auto delegate_id = balance_rec.condition.delegate_id;
                              auto delegate_rec = _client->get_chain()->get_account_record( delegate_id );
                              string sign = (delegate_id > 0 ? "+" : "-");
                              if (delegate_rec->name.size() > 21)
                              {
                                  if( _out ) (*_out) << std::setw(25) << sign << delegate_rec->name.substr(0, 21) << "...";
                              }
                              else
                              {
                                  if( _out ) (*_out) << std::setw(25) << sign << delegate_rec->name;
                              }
                              break;
                          }
                          default:
                          {
                              FC_ASSERT(!"unimplemented condition type");
                          }
                      } // switch cond type
                      if( _out ) (*_out) << "\n";
                  } // for balance in balances
              }
              else if (method_name == "blockchain_list_registered_accounts")
              {
                  string start = "";
                  int32_t count = 25; // In CLI this is a more sane result
                  if (arguments.size() > 0)
                      start = arguments[0].as_string();
                  if (arguments.size() > 1)
                      count = arguments[1].as<int32_t>();
                  print_registered_account_list( result.as<vector<account_record>>(), count );
              }
              else if (method_name == "blockchain_list_delegates")
              {
                  auto delegates = result.as<vector<account_record>>();
                  uint32_t current = 0;
                  uint32_t count = 1000000;
                  if (arguments.size() > 0)
                      current = arguments[0].as<uint32_t>();
                  if (arguments.size() > 1)
                      count = arguments[1].as<uint32_t>();
                  auto max = current + count;
                  auto num_active = BTS_BLOCKCHAIN_NUM_DELEGATES - current;

                  if( _out ) (*_out) << std::setw(12) << "ID";
                  if( _out ) (*_out) << std::setw(25) << "NAME";
                  if( _out ) (*_out) << std::setw(20) << "NET VOTES";
                  if( _out ) (*_out) << "\n---------------------------------------------------------\n";

                  if (current < num_active)
                      if( _out ) (*_out) << "** Active:\n";

                  auto total_delegates = delegates.size();
                  for( auto delegate_rec : delegates )
                  {
                      if (current > max || current == total_delegates)
                          return;
                      if (current == num_active)
                          if( _out ) (*_out) << "** Inactive:\n"; 

                      if( _out ) (*_out) << std::setw(12) << delegate_rec.id;
                      if( _out ) (*_out) << std::setw(25) << delegate_rec.name;
                      std::stringstream ss;
                      auto opt_rec = _client->get_chain()->get_asset_record(asset_id_type(0));
                      FC_ASSERT(opt_rec.valid(), "No asset with id 0??");
                      float percent = 100.0 * delegate_rec.net_votes() / opt_rec->current_share_supply;
                      ss << percent;
                      ss << " %";
                      if( _out ) (*_out) << std::setw(20) << ss.str();
                      if( _out ) (*_out) << "\n";
                      current++;
                  }
                  if( _out ) (*_out) << "  Use \"blockchain_list_delegates <start_num> <count>\" to see more.\n";
              }
              else if (method_name == "blockchain_get_proposal_votes")
              {
                  auto votes = result.as<vector<proposal_vote>>();
                  if( _out ) (*_out) << std::left;
                  if( _out ) (*_out) << std::setw(15) << "DELEGATE";
                  if( _out ) (*_out) << std::setw(22) << "TIME";
                  if( _out ) (*_out) << std::setw(5)  << "VOTE";
                  if( _out ) (*_out) << std::setw(35) << "MESSAGE";
                  if( _out ) (*_out) << "\n----------------------------------------------------------------";
                  if( _out ) (*_out) << "-----------------------\n";
                  for (auto vote : votes)
                  {
                      auto time = boost::posix_time::from_time_t(time_t(vote.timestamp.sec_since_epoch()));
                      auto rec = _client->get_chain()->get_account_record( vote.id.delegate_id );
                      if( _out ) (*_out) << std::setw(15) << pretty_shorten(rec->name, 14);
                      if( _out ) (*_out) << std::setw(20) << boost::posix_time::to_iso_extended_string( time );
                      if (vote.vote == proposal_vote::no)
                      {
                          if( _out ) (*_out) << std::setw(5) << "NO";
                      }
                      else if (vote.vote == proposal_vote::yes)
                      {
                          if( _out ) (*_out) << std::setw(5) << "YES";
                      }
                      else
                      {
                          if( _out ) (*_out) << std::setw(5) << "??";
                      }
                      if( _out ) (*_out) << std::setw(35) << pretty_shorten(vote.message, 35);
                      if( _out ) (*_out) << "\n";
                  }
                  if( _out ) (*_out) << "\n";
              }
              else if (method_name == "blockchain_list_proposals")
              {
                  auto proposals = result.as<vector<proposal_record>>();
                  if( _out ) (*_out) << std::left;
                  if( _out ) (*_out) << std::setw(10) << "ID";
                  if( _out ) (*_out) << std::setw(20) << "SUBMITTED BY";
                  if( _out ) (*_out) << std::setw(22) << "SUBMIT TIME";
                  if( _out ) (*_out) << std::setw(15) << "TYPE";
                  if( _out ) (*_out) << std::setw(20) << "SUBJECT";
                  if( _out ) (*_out) << std::setw(35) << "BODY";
                  if( _out ) (*_out) << std::setw(20) << "DATA";
                  if( _out ) (*_out) << std::setw(10)  << "RATIFIED";
                  if( _out ) (*_out) << "\n------------------------------------------------------------";
                  if( _out ) (*_out) << "-----------------------------------------------------------------";
                  if( _out ) (*_out) << "------------------\n";
                  for (auto prop : proposals)
                  {
                      if( _out ) (*_out) << std::setw(10) << prop.id;
                      auto delegate_rec = _client->get_chain()->get_account_record(prop.submitting_delegate_id);
                      if( _out ) (*_out) << std::setw(20) << pretty_shorten(delegate_rec->name, 19);
                      auto time = boost::posix_time::from_time_t(time_t(prop.submission_date.sec_since_epoch()));
                      if( _out ) (*_out) << std::setw(20) << boost::posix_time::to_iso_extended_string( time );
                      if( _out ) (*_out) << std::setw(15) << pretty_shorten(prop.proposal_type, 14);
                      if( _out ) (*_out) << std::setw(20) << pretty_shorten(prop.subject, 19);
                      if( _out ) (*_out) << std::setw(35) << pretty_shorten(prop.body, 34);
                      if( _out ) (*_out) << std::setw(20) << pretty_shorten(fc::json::to_pretty_string(prop.data), 19);
                      if( _out ) (*_out) << std::setw(10) << (prop.ratified ? "YES" : "NO");
                  }
                  if( _out ) (*_out) << "\n"; 
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
                  {
                    if( _out ) (*_out) << (string)result.as<bts::blockchain::asset>() << "\n";
                  }
                  else if (result_type == "address")
                  {
                    if( _out ) (*_out) << (string)result.as<bts::blockchain::address>() << "\n";
                  }
                  else if (result_type == "null" || result_type == "void")
                  {
                    if( _out ) (*_out) << "OK\n";
                  }
                  else
                  {
                    if( _out ) (*_out) << fc::json::to_pretty_string(result) << "\n";
                  }
                }
                catch( const fc::key_not_found_exception& )
                {
                   elog( " KEY NOT FOUND " );
                   if( _out ) (*_out) << "key not found \n";
                }
                catch( ... )
                {
                   if( _out ) (*_out) << "unexpected exception \n";
                }
              }
            }

            string pretty_shorten(const string& str, uint32_t size)
            {
                if (str.size() < size)
                    return str;
                return str.substr(0, size - 3) + "...";
            }

            void print_contact_account_list(const std::vector<wallet_account_record> account_records)
            {
                if( _out ) (*_out) << std::setw( 25 ) << std::left << "NAME";
                if( _out ) (*_out) << std::setw( 64 ) << "KEY";
                if( _out ) (*_out) << std::setw( 22 ) << "REGISTERED";
                if( _out ) (*_out) << std::setw( 15 ) << "TRUST LEVEL";
                if( _out ) (*_out) << "\n";

                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 14) + " (delegate)";
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 24);
                    }

                    if( _out ) (*_out) << std::setw(64) << string( acct.active_key() );

                    if( acct.id == 0 )
                    {
                       if( _out ) (*_out) << std::setw( 22 ) << "NO";
                    } 
                    else 
                    {
                        if( _out ) (*_out) << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }
                    
                    if( _out ) (*_out) << std::setw( 10) << acct.trust_level;
                    if( _out ) (*_out) << "\n";
                }
            }


            void print_receive_account_list(const vector<wallet_account_record>& account_records)
            {
                if( _out ) (*_out) << std::setw( 25 ) << std::left << "NAME";
                if( _out ) (*_out) << std::setw( 25 ) << std::left << "BALANCE";
                if( _out ) (*_out) << std::setw( 64 ) << "KEY";
                if( _out ) (*_out) << std::setw( 22 ) << "REGISTERED";
                if( _out ) (*_out) << std::setw( 15 ) << "TRUST LEVEL";
                if( _out ) (*_out) << "\n";

                //if( _out ) (*_out) << fc::json::to_string( account_records ) << "\n";

                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 14) + " (delegate)";
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 24);
                    }

                    auto balance = _client->get_wallet()->get_balance( BTS_ADDRESS_PREFIX, acct.name );
                    if( _out ) (*_out) << std::setw(25) << _client->get_chain()->to_pretty_asset(balance[0]);

                    if( _out ) (*_out) << std::setw(64) << string( acct.active_key() );

                    if (acct.id == 0 ) 
                    { //acct.registration_date == fc::time_point_sec()) {
                        if( _out ) (*_out) << std::setw( 22 ) << "NO";
                    } 
                    else 
                    {
                        if( _out ) (*_out) << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }

                    if( _out ) (*_out) << std::setw( 15 ) << acct.trust_level;
                    if( _out ) (*_out) << "\n";
                }
            }

            void print_registered_account_list(const vector<account_record> account_records, int32_t count )
            {
                if( _out ) (*_out) << std::setw( 25 ) << std::left << "NAME";
                if( _out ) (*_out) << std::setw( 64 ) << "KEY";
                if( _out ) (*_out) << std::setw( 22 ) << "REGISTERED";
                if( _out ) (*_out) << std::setw( 15 ) << "VOTES FOR";
                if( _out ) (*_out) << std::setw( 15 ) << "VOTES AGAINST";
                if( _out ) (*_out) << std::setw( 15 ) << "TRUST LEVEL";

                if( _out ) (*_out) << "\n";
                auto counter = 0;
                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 14) + " (delegate)";
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw(25) << pretty_shorten(acct.name, 24);
                    }
                    
                    if( _out ) (*_out) << std::setw(64) << string( acct.active_key() );
                    if( _out ) (*_out) << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                                    boost::posix_time::from_time_t( time_t( acct.registration_date.sec_since_epoch() ) ) );

                    if ( acct.is_delegate() )
                    {
                        if( _out ) (*_out) << std::setw(15) << acct.delegate_info->votes_for;
                        if( _out ) (*_out) << std::setw(15) << acct.delegate_info->votes_against;
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw(15) << "N/A";
                        if( _out ) (*_out) << std::setw(15) << "N/A";
                    }
                    if ( ! _client->get_wallet()->is_open() )
                    {
                        if( _out ) (*_out) << "?? (wallet closed)";
                    }
                    else
                    {
                        auto trust = _client->get_wallet()->get_delegate_trust_level( acct.name );
                        if( _out ) (*_out) << std::setw( 15 ) << trust;
                    }

                    if( _out ) (*_out) << "\n";

                    /* Count is positive b/c CLI overrides default -1 arg */
                    if (counter >= count)
                    {
                        if( _out ) (*_out) << "... Use \"blockchain_list_registered_accounts <start_name> <count>\" to see more.\n";
                        return;
                    }
                    counter++;

                }
            }

            void print_transaction_history(const std::vector<bts::wallet::pretty_transaction> txs)
            {
                /* Print header */
                if( _out ) (*_out) << std::setw(  3 ) << std::right << "#";
                if( _out ) (*_out) << std::setw(  7 ) << "BLK" << ".";
                if( _out ) (*_out) << std::setw(  5 ) << std::left << "TRX";
                if( _out ) (*_out) << std::setw( 20 ) << "TIMESTAMP";
                if( _out ) (*_out) << std::setw( 20 ) << "FROM";
                if( _out ) (*_out) << std::setw( 20 ) << "TO";
                if( _out ) (*_out) << std::setw( 25 ) << "MEMO";
                if( _out ) (*_out) << std::setw( 20 ) << std::right << "AMOUNT";
                if( _out ) (*_out) << std::setw( 20 ) << "FEE";
                if( _out ) (*_out) << std::setw( 14 ) << "VOTE";
                if( _out ) (*_out) << std::setw( 42 ) << "ID";
                if( _out ) (*_out) << "\n---------------------------------------------------------------------------------------------------";
                if( _out ) (*_out) <<   "--------------------------------------------------------------------------------------------------\n";
                if( _out ) (*_out) << std::right; 
                
                int count = 1;
                for( auto tx : txs )
                {
                    /* Print index */
                    if( _out ) (*_out) << std::setw( 3 ) << count;
                    count++;

                    /* Print block and transaction numbers */
                    if (tx.block_num == -1)
                    {
                        if( _out ) (*_out) << std::setw( 13 ) << std::left << "   pending";
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw( 7 ) << tx.block_num << ".";
                        if( _out ) (*_out) << std::setw( 5 ) << std::left << tx.trx_num;
                    }

                    /* Print timestamp */
                    if( _out ) (*_out) << std::setw( 20 ) << boost::posix_time::to_iso_extended_string( boost::posix_time::from_time_t( tx.received_time ) );

                    // Print "from" account
                    if( _out ) (*_out) << std::setw( 20 ) << pretty_shorten(tx.from_account, 19);
                    
                    // Print "to" account
                    if( _out ) (*_out) << std::setw( 20 ) << pretty_shorten(tx.to_account, 19);

                    // Print "memo" on transaction
                    if( _out ) (*_out) << std::setw( 25 ) << pretty_shorten(tx.memo_message, 24);

                    /* Print amount */
                    {
                        if( _out ) (*_out) << std::right;
                        std::stringstream ss;
                        ss << _client->get_chain()->to_pretty_asset(tx.amount);
                        if( _out ) (*_out) << std::setw( 20 ) << ss.str();
                    }

                    /* Print fee */
                    {
                        if( _out ) (*_out) << std::right;
                        std::stringstream ss;
                        ss << _client->get_chain()->to_pretty_asset( asset(tx.fees,0 ));
                        if( _out ) (*_out) << std::setw( 20 ) << ss.str();
                    }

                    if( _out ) (*_out) << std::right;
                    /* Print delegate vote */
                    bool has_deposit = false;
                    std::stringstream ss;
                    for (auto op : tx.operations)
                    {
                        if (op.get_object()["op_name"].as<string>() == "deposit")
                        {
                            has_deposit = true;
                            auto vote = op.as<pretty_deposit_op>().vote;
                            if( vote.first > 0 )
                                ss << "+";
                            else if (vote.first < 0)
                                ss << "-";
                            else
                                ss << " ";
                            ss << vote.second;
                            break;
                        }
                    }
                    if (has_deposit)
                    {
                        if( _out ) (*_out) << std::setw(14) << ss.str();
                    }
                    else
                    {
                        if( _out ) (*_out) << std::setw(14) << "N/A";
                    }

                    if( _out ) (*_out) << std::right;
                    /* Print transaction ID */
                    if( _out ) (*_out) << std::setw( 42 ) << string( tx.trx_id );

                    if( _out ) (*_out) << std::right << "\n";
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
    extern "C" int control_c_handler(int count, int key);
#endif

    cli_impl::cli_impl(const client_ptr& client, std::istream* input_stream, std::ostream* output_stream) : 
      _client(client),
      _rpc_server(client->get_rpc_server()),
      _input_stream(input_stream), 
      _out(output_stream),
      _quit(false),
      show_raw_output(false)
    {
#ifdef HAVE_READLINE
      //if( &output_stream == &std::cout ) // readline
      {
         cli_impl_instance = this;
         _method_data_is_initialized = false;
         rl_attempted_completion_function = &json_completion_function;
      }
#ifndef __APPLE__
      // TODO: find out why this isn't defined on APPL
      //rl_bind_keyseq("\\C-c", &control_c_handler);
#endif
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
    extern "C" int control_c_handler(int count, int key)
    {
       std::cout << "\n\ncontrol-c!\n\n";
      return 0;
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

  cli::cli( const client_ptr& client, std::istream* input_stream, std::ostream* output_stream)
  :my( new detail::cli_impl(client,input_stream,output_stream) )
  {
    my->_self = this;
  }

  void cli::set_input_log_stream(boost::optional<std::ostream&> input_log_stream)
  {
    my->_input_log_stream = input_log_stream;
  } 
  
  void cli::process_commands()
  {
    ilog( "starting to process interactive commands" );
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
     if( my->_cin_complete.valid() )
     {
        my->_cin_complete.cancel();
        if( my->_cin_complete.ready() )
           my->_cin_complete.wait();
     }
     my->_rpc_server->wait_on_quit();
  }

  void cli::quit()
  {
    my->_cin_complete.cancel();
  }

  bool cli::execute_command_line( const string& line, std::ostream* output)
  {
    auto old_output = my->_out;
    auto old_input = my->_input_stream;
    bool result = false;
    if( output )
    {
        my->_out = output;
        my->_input_stream = nullptr;
    }
    result = my->execute_command_line(line);
    if( output )
    {
        my->_out = old_output;
        my->_input_stream = old_input;
    }
    return result;
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
  void cli::format_and_print_result(const string& command,
                                    const fc::variants& arguments,
                                    const fc::variant& result)
  {
    return my->format_and_print_result(command, arguments, result);
  }

} } // bts::cli
