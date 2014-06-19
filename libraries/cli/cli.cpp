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
# include <readline/readline.h>
# include <readline/history.h>
// I don't know exactly what version of readline we need.  I know the 4.2 version that ships on some macs is
// missing some functions we require.  We're developing against 6.3, but probably anything in the 6.x 
// series is fine
# if RL_VERSION_MAJOR < 6
#  ifdef _MSC_VER
#   pragma message("You have an old version of readline installed that might not support some of the features we need")
#   pragma message("Readline support will not be compiled in")
#  else
#   warning "You have an old version of readline installed that might not support some of the features we need"
#   warning "Readline support will not be compiled in"
#  endif
#  undef HAVE_READLINE
# endif
#endif

namespace bts { namespace cli {

  namespace detail
  {
      class cli_impl
      {
         public:
            client_ptr                                      _client;
            rpc_server_ptr                                  _rpc_server;
            bts::cli::cli*                                  _self;
            fc::thread                                      _cin_thread;
                                                            
            bool                                            _quit;
            bool                                            show_raw_output;
            bool                                            _daemon_mode;

            boost::iostreams::stream< boost::iostreams::null_sink > nullstream;
            
            std::ostream*                  _out;   //cout | log_stream | tee(cout,log_stream) | null_stream
            std::istream*                  _command_script;
            std::istream*                  _input_stream;
            boost::optional<std::ostream&> _input_stream_log;
            cli_impl(const client_ptr& client, std::istream* command_script, std::ostream* output_stream);

            void process_commands(std::istream* input_stream);

            void start()
              {
                try
                {
                  if (_command_script)
                    process_commands(_command_script);
                  if (_daemon_mode)
                    _rpc_server->wait_till_rpc_server_shutdown();
                  else if (!_quit)
                    process_commands(&std::cin);
                  _rpc_server->shutdown_rpc_server();
                }
                catch ( const fc::exception& e)
                {
                    *_out << "\nshutting down\n";
                    elog( "${e}", ("e",e.to_detail_string() ) );
                    _rpc_server->shutdown_rpc_server();
                }
              }

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
              if( command.size() == 0 )
                 return;
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
                // NOTE: arguments here have not been filtered for private keys or passwords
                // ilog( "command: ${c} ${a}", ("c",command)("a",arguments) ); 
                command_is_valid = true;
              }
              catch( const rpc::unknown_method& )
              {
                 if( command.size() )
                   *_out << "Error: invalid command \"" << command << "\"\n";
              }
              catch( const fc::canceled_exception&)
              {
                *_out << "Command aborted.\n";
              }
              catch( const fc::exception& e)
              {
                *_out << e.to_detail_string() <<"\n";
                *_out << "Error parsing command \"" << command << "\": " << e.to_string() << "\n";
                arguments = fc::variants { command };
                edump( (e.to_detail_string()) );
                auto usage = _client->help( command ); //_rpc_server->direct_invoke_method("help", arguments).as_string();
                *_out << usage << "\n";
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
                  *_out << e.to_detail_string() << "\n";
                }
              }
            } //parse_and_execute_interactive_command

            bool execute_command_line(const string& line)
            { try {
                     wdump( (line));
            
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
                  *_out << "Command aborted\n";
                }
              } //end if command line not empty
              return true;
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",line) ) }


            string get_line( const string& prompt = CLI_PROMPT_SUFFIX, bool no_echo = false)
            {
                  if( _quit ) return std::string();
                  if( _input_stream == nullptr )
                     FC_CAPTURE_AND_THROW( fc::canceled_exception ); //_input_stream != nullptr );

                  //FC_ASSERT( _self->is_interactive() );
                  string line;
                  if ( no_echo )
                  {
                      *_out << prompt;
                      // there is no need to add input to history when echo is off, so both Windows and Unix implementations are same
                      fc::set_console_echo(false);
                      _cin_thread.async([this,&line](){ std::getline( *_input_stream, line ); }).wait();
                      fc::set_console_echo(true);
                      *_out << std::endl;
                  }
                  else
                  {
                  #ifdef HAVE_READLINE 
                    if (_input_stream == &std::cin)
                    {
                      char* line_read = nullptr;
                      _out->flush(); //readline doesn't use cin, so we must manually flush _out
                      line_read = readline(prompt.c_str());
                      if(line_read && *line_read)
                          add_history(line_read);
                      if( line_read == nullptr )
                         FC_THROW_EXCEPTION( fc::eof_exception, "" );
                      line = line_read;
                      free(line_read);
                    }
                  else
                    {
                      *_out <<prompt;
                      _cin_thread.async([this,&line](){ std::getline( *_input_stream, line ); }).wait();
                    }
                  #else
                    *_out <<prompt;
                    _cin_thread.async([this,&line](){ std::getline( *_input_stream, line ); }).wait();
                  #endif
                  if (_input_stream_log)
                    {
                    _out->flush();
                    *_input_stream_log << line << std::endl;
                    }
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
            { try {
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
                  {
                    return arguments;
                  }
                  else //if missing required argument, prompt for that argument
                  {
                    const bts::api::parameter_data& this_parameter = method_data.parameters[i];
                    string prompt = this_parameter.name /*+ "(" + this_parameter.type  + ")"*/ + ": ";

                    //if we're prompting for a password, don't echo it to console
                    bool is_new_passphrase = (this_parameter.type == "new_passphrase");
                    bool is_passphrase = (this_parameter.type == "passphrase") || is_new_passphrase;
                    if (is_passphrase)
                    {
                      auto passphrase = prompt_for_input( this_parameter.name, is_passphrase, is_new_passphrase );
                      arguments.push_back(fc::variant(passphrase));
                    }
                    else //not a passphrase
                    {
                      string prompt_answer = get_line(prompt, is_passphrase );
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
            } FC_CAPTURE_AND_RETHROW() }

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
                   // *_out << "ignoring eof  line: "<<__LINE__<<"\n";
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
              if(command == "wallet_rescan_blockchain")
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

                  *_out << "Rescanning blockchain...\n";
                  uint32_t start;
                  if (arguments.size() == 0)
                      start = 1;
                  else
                      start = arguments[0].as<uint32_t>();
                  while(true)
                  {
                      try {
                          *_out << "|";
                          for(int i = 0; i < 100; i++)
                              *_out << "-";
                          *_out << "|\n|=";
                          uint32_t next_step = 0;
                          auto cb = [=](uint32_t cur,
                                                       uint32_t last
                                                       ) mutable
                          {
                              if (start > last || cur >= last) // if WTF
                                  return;
                              if (((100*(1 + cur - start)) / (1 + last - start)) > next_step)
                              {
                                  *_out << "=";
                                  next_step++;
                              }
                          };
                          _client->get_wallet()->scan_chain(start, -1, cb);
                          *_out << "|\n";
                          *_out << "Scan complete.\n";
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

            string prompt_for_input( const string& prompt, bool no_echo = false, bool verify = false )
            { try {
                string input;
                while( true )
                {
                    input = get_line( prompt + ": ", no_echo );
                    if( input.empty() ) FC_THROW_EXCEPTION(fc::canceled_exception, "input aborted");

                    if( verify )
                    {
                        if( input != get_line( prompt + " (verify): ", no_echo ) )
                        {
                            *_out << "Input did not match, try again\n";
                            continue;
                        }
                    }
                    break;
                }
                return input;
            } FC_CAPTURE_AND_RETHROW( (prompt)(no_echo) ) }

            void interactive_open_wallet()
            {
              if( _client->get_wallet()->is_open() ) 
                return;

              *_out << "A wallet must be open to execute this command. You can:\n";
              *_out << "(o) Open an existing wallet\n";
              *_out << "(c) Create a new wallet\n";
              *_out << "(q) Abort command\n";

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
                  *_out << "Wrong answer!\n";
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
                    *_out << (string)result.as<bts::blockchain::asset>() << "\n";
                  }
                  else if (result_type == "address")
                  {
                    *_out << (string)result.as<bts::blockchain::address>() << "\n";
                  }
                  else if (result_type == "null" || result_type == "void")
                  {
                    *_out << "OK\n";
                  }
                  else
                  {
                    *_out << fc::json::to_pretty_string(result) << "\n";
                  }
              }
             
              if (method_name == "help")
              {
                string help_string = result.as<string>();
                *_out << help_string << "\n";
              }
              else if( method_name == "wallet_account_vote_summary" )
              {
                  if( !_out ) return;

                     (*_out) << std::setw(25) << std::left << "Delegate"
                            << std::setw(15) << "For"
                            << std::setw(15) << "Against\n";
                     (*_out) <<"--------------------------------------------------------------\n";
                  auto votes = result.as<bts::wallet::wallet::account_vote_summary_type>();
                  for( auto vote : votes )
                  {
                     (*_out) << std::setw(25) << vote.first 
                            << std::setw(15) << vote.second.votes_for 
                            << std::setw(15) << vote.second.votes_against <<"\n";
                  }
              }
              else if (method_name == "list_errors")
              {
                  auto error_map = result.as<map<fc::time_point,fc::exception> >();
                  for( auto item : error_map )
                  {
                     (*_out) << string(item.first) << " (" << fc::get_approximate_relative_time_string( item.first ) << " )\n";
                     (*_out) << item.second.to_detail_string();
                     (*_out) << "\n";
                  }
              }
              else if (method_name == "wallet_account_transaction_history")
              {
                  auto tx_history_summary = result.as<vector<pretty_transaction>>();
                  print_transaction_history(tx_history_summary);
              }
              else if( method_name == "wallet_market_order_list" )
              {
                  auto order_list = result.as<vector<market_order_status> >();
                  print_wallet_market_order_list( order_list );
              }
              else if ( command == "wallet_list_receive_accounts" )
              {
                  auto accts = result.as<vector<wallet_account_record>>();
                  print_receive_account_list( accts );
              }
              else if ( command == "wallet_list_contact_accounts" )
              {
                  auto accts = result.as<vector<wallet_account_record>>();
                  print_contact_account_list( accts );
              }
              else if (method_name == "wallet_account_balance" )
              {
                 auto bc = _client->get_chain();
                  auto summary = result.as<unordered_map<string, map<string, share_type>>>();
                  for( auto accts : summary )
                  {
                      *_out << accts.first << ":\n";
                      for( auto balance : accts.second )
                      {
                         *_out << "    " << bc->to_pretty_asset( asset( balance.second, bc->get_asset_id( balance.first) ) ) <<"\n";//fc::to_pretty_string(balance.second) << " " << balance.first <<"\n"; 
                      }
                  }
              }
              else if (method_name == "wallet_transfer")
              {
                  auto trx = result.as<signed_transaction>();
                  print_transfer_summary( trx );
              }
              else if (method_name == "wallet_list")
              {
                  auto wallets = result.as<vector<string>>();
                  if (wallets.empty())
                      *_out << "No wallets found.\n";
                  else
                      for (auto wallet : wallets)
                          *_out << wallet << "\n";
              }
              else if (method_name == "wallet_list_unspent_balances" )
              {
                  auto balance_recs = result.as<vector<wallet_balance_record>>();
                  print_unspent_balances(balance_recs);
              }
              else if (method_name == "blockchain_list_registered_accounts")
              {
                  string start = "";
                  int32_t count = 25; // In CLI this is a more sane default
                  if (arguments.size() > 0)
                      start = arguments[0].as_string();
                  if (arguments.size() > 1)
                      count = arguments[1].as<int32_t>();
                  print_registered_account_list( result.as<vector<account_record>>(), count );
              }
              else if (method_name == "blockchain_list_registered_assets")
              {
                  auto records = result.as<vector<asset_record>>();
                  *_out << std::setw(6) << "ID";
                  *_out << std::setw(7) << "SYMBOL";
                  *_out << std::setw(15) << "NAME";
                  *_out << std::setw(35) << "DESCRIPTION";
                  *_out << std::setw(16) << "CURRENT_SUPPLY";
                  *_out << std::setw(16) << "MAX_SUPPLY";
                  *_out << std::setw(16) << "FEES COLLECTED";
                  *_out << std::setw(16) << "REGISTERED";
                  *_out << "\n";
                  for (auto asset : records)
                  {
                      *_out << std::setprecision(15);
                      *_out << std::setw(6) << asset.id;
                      *_out << std::setw(7) << asset.symbol;
                      *_out << std::setw(15) << pretty_shorten(asset.name, 14);
                      *_out << std::setw(35) << pretty_shorten(asset.description, 33);
                      *_out << std::setw(16) << double(asset.current_share_supply) / asset.precision;
                      *_out << std::setw(16) << double(asset.maximum_share_supply) / asset.precision;
                      *_out << std::setw(16) << double(asset.collected_fees) / asset.precision;
                      auto time = boost::posix_time::from_time_t(time_t(asset.registration_date.sec_since_epoch()));
                      *_out << std::setw(16) << boost::posix_time::to_iso_extended_string( time );
                      *_out << "\n";
                  }
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

                  *_out << std::setw(12) << "ID";
                  *_out << std::setw(25) << "NAME";
                  *_out << std::setw(20) << "VOTES FOR";
                  *_out << std::setw(20) << "VOTES AGAINST";
                  *_out << std::setw(20) << "NET VOTES";
                  *_out << std::setw(16) << "BLOCKS PRODUCED";
                  *_out << std::setw(16) << "BLOCKS MISSED";
                  *_out << "\n---------------------------------------------------------\n";

                  if (current < num_active)
                      *_out << "** Active:\n";

                  auto total_delegates = delegates.size();
                  for( auto delegate_rec : delegates )
                  {
                      if (current > max || current == total_delegates)
                          return;
                      if (current == num_active)
                          *_out << "** Standby:\n";

                      *_out << std::setw(12) << delegate_rec.id;
                      *_out << std::setw(25) << delegate_rec.name;
                      
                      *_out << std::setw(20) << delegate_rec.votes_for();
                      *_out << std::setw(20) << delegate_rec.votes_against();


                      std::stringstream ss;
                      auto opt_rec = _client->get_chain()->get_asset_record(asset_id_type(0));
                      FC_ASSERT(opt_rec.valid(), "No asset with id 0??");
                      float percent = 100.0 * delegate_rec.net_votes() / opt_rec->current_share_supply;
                      ss << std::setprecision(10);
                      ss << percent;
                      ss << " %";
                      *_out << std::setw(20) << ss.str();

                      *_out << std::setw(16) << delegate_rec.delegate_info->blocks_produced;
                      *_out << std::setw(16) << delegate_rec.delegate_info->blocks_missed;

                      *_out << "\n";
                      current++;
                  }
                  *_out << "  Use \"blockchain_list_delegates <start_num> <count>\" to see more.\n";
              }
              else if (method_name == "blockchain_get_proposal_votes")
              {
                  auto votes = result.as<vector<proposal_vote>>();
                  *_out << std::left;
                  *_out << std::setw(15) << "DELEGATE";
                  *_out << std::setw(22) << "TIME";
                  *_out << std::setw(5)  << "VOTE";
                  *_out << std::setw(35) << "MESSAGE";
                  *_out << "\n----------------------------------------------------------------";
                  *_out << "-----------------------\n";
                  for (auto vote : votes)
                  {
                      auto time = boost::posix_time::from_time_t(time_t(vote.timestamp.sec_since_epoch()));
                      auto rec = _client->get_chain()->get_account_record( vote.id.delegate_id );
                      *_out << std::setw(15) << pretty_shorten(rec->name, 14);
                      *_out << std::setw(20) << boost::posix_time::to_iso_extended_string( time );
                      if (vote.vote == proposal_vote::no)
                      {
                          *_out << std::setw(5) << "NO";
                      }
                      else if (vote.vote == proposal_vote::yes)
                      {
                          *_out << std::setw(5) << "YES";
                      }
                      else
                      {
                          *_out << std::setw(5) << "??";
                      }
                      *_out << std::setw(35) << pretty_shorten(vote.message, 35);
                      *_out << "\n";
                  }
                  *_out << "\n";
              }
              else if (method_name.find("blockchain_get_account_record") != std::string::npos)
              {
                  // Pretty print of blockchain_get_account_record{,_by_id}
                  bts::blockchain::account_record record;
                  try
                  {
                      record = result.as<bts::blockchain::account_record>();
                  }
                  catch (...)
                  {
                      *_out << "No record found.\n";
                      return;
                  }
                  *_out << "Record for '" << record.name << "' -- Registered on ";
                  *_out << boost::posix_time::to_simple_string(
                             boost::posix_time::from_time_t(time_t(record.registration_date.sec_since_epoch())));
                  *_out << ", last update was " << fc::get_approximate_relative_time_string(record.last_update) << "\n";
                  *_out << "Owner's key: " << std::string(record.owner_key) << "\n";

                  //Only print active key history if there are keys in the history which are not the owner's key above.
                  if (record.active_key_history.size() > 1 || record.active_key_history.begin()->second != record.owner_key)
                  {
                      *_out << "History of active keys:\n";

                      for (auto key : record.active_key_history)
                          *_out << "  Key " << std::string(key.second) << " last used " << fc::get_approximate_relative_time_string(key.first) << "\n";
                  }

                  if (record.is_delegate())
                  {
                      *_out << std::left;
                      *_out << std::setw(20) << "VOTES FOR";
                      *_out << std::setw(20) << "VOTES AGAINST";
                      *_out << std::setw(20) << "NET VOTES";
                      *_out << std::setw(16) << "BLOCKS PRODUCED";
                      *_out << std::setw(16) << "BLOCKS MISSED";
                      *_out << std::setw(16) << "LAST BLOCK #";
                      *_out << std::setw(20) << "TOTAL PAY";
                      _out->put('\n');
                      for (int i=0; i < 128; ++i)
                          _out->put('-');
                      _out->put('\n');

                      auto supply = _client->get_chain()->get_asset_record(bts::blockchain::asset_id_type(0))->current_share_supply;
                                                //Horribly painful way to print a % after a double with precision of 8. Better solutions welcome.
                      *_out << std::setw(20) << (fc::variant(double(record.votes_for())*100.0 / supply).as_string().substr(0,10) + '%')
                            << std::setw(20) << (fc::variant(double(record.votes_against())*100.0 / supply).as_string().substr(0,10) + '%')
                            << std::setw(20) << (fc::variant(double(record.net_votes())*100.0 / supply).as_string().substr(0,10) + '%')
                            << std::setw(16) << record.delegate_info->blocks_produced
                            << std::setw(16) << record.delegate_info->blocks_missed
                            << std::setw(16) << record.delegate_info->last_block_num_produced
                            << _client->get_chain()->to_pretty_asset(asset(record.delegate_pay_balance()))
                            << "\n";
                  }
                  else
                  {
                      *_out << "This account is not a delegate.\n";
                  }

                  if(record.meta_data)
                      *_out << "Public data:\n" << fc::json::to_pretty_string(record.public_data);
              }
              else if (method_name == "blockchain_list_proposals")
              {
                  auto proposals = result.as<vector<proposal_record>>();
                  *_out << std::left;
                  *_out << std::setw(10) << "ID";
                  *_out << std::setw(20) << "SUBMITTED BY";
                  *_out << std::setw(22) << "SUBMIT TIME";
                  *_out << std::setw(15) << "TYPE";
                  *_out << std::setw(20) << "SUBJECT";
                  *_out << std::setw(35) << "BODY";
                  *_out << std::setw(20) << "DATA";
                  *_out << std::setw(10)  << "RATIFIED";
                  *_out << "\n------------------------------------------------------------";
                  *_out << "-----------------------------------------------------------------";
                  *_out << "------------------\n";
                  for (auto prop : proposals)
                  {
                      *_out << std::setw(10) << prop.id;
                      auto delegate_rec = _client->get_chain()->get_account_record(prop.submitting_delegate_id);
                      *_out << std::setw(20) << pretty_shorten(delegate_rec->name, 19);
                      auto time = boost::posix_time::from_time_t(time_t(prop.submission_date.sec_since_epoch()));
                      *_out << std::setw(20) << boost::posix_time::to_iso_extended_string( time );
                      *_out << std::setw(15) << pretty_shorten(prop.proposal_type, 14);
                      *_out << std::setw(20) << pretty_shorten(prop.subject, 19);
                      *_out << std::setw(35) << pretty_shorten(prop.body, 34);
                      *_out << std::setw(20) << pretty_shorten(fc::json::to_pretty_string(prop.data), 19);
                      *_out << std::setw(10) << (prop.ratified ? "YES" : "NO");
                  }
                  *_out << "\n"; 
              }
              else if (method_name == "network_list_potential_peers")
              {
                  auto peers = result.as<std::vector<net::potential_peer_record>>();
                  *_out << std::setw(25) << "ENDPOINT";
                  *_out << std::setw(25) << "LAST SEEN";
                  *_out << std::setw(25) << "LAST CONNECT ATTEMPT";
                  *_out << std::setw(30) << "SUCCESSFUL CONNECT ATTEMPTS";
                  *_out << std::setw(30) << "FAILED CONNECT ATTEMPTS";
                  *_out << std::setw(35) << "LAST CONNECTION DISPOSITION";
                  *_out << std::setw(30) << "LAST ERROR";

                  *_out<< "\n";
                  for (auto peer : peers)
                  {
                      *_out<< std::setw(25) << string(peer.endpoint);
                      auto seen_time = boost::posix_time::from_time_t(time_t(peer.last_seen_time.sec_since_epoch()));
                      *_out<< std::setw(25) << boost::posix_time::to_iso_extended_string( seen_time );
                      auto connect_time = boost::posix_time::from_time_t(time_t(peer.last_connection_attempt_time.sec_since_epoch()));
                      *_out << std::setw(25) << boost::posix_time::to_iso_extended_string( connect_time );
                      *_out << std::setw(30) << peer.number_of_successful_connection_attempts;
                      *_out << std::setw(30) << peer.number_of_failed_connection_attempts;
                      *_out << std::setw(35) << string( peer.last_connection_disposition );
                      *_out << std::setw(30) << (peer.last_error ? peer.last_error->to_detail_string() : "none");

                      *_out << "\n";
                  }
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
                    *_out << (string)result.as<bts::blockchain::asset>() << "\n";
                  }
                  else if (result_type == "address")
                  {
                    *_out << (string)result.as<bts::blockchain::address>() << "\n";
                  }
                  else if (result_type == "null" || result_type == "void")
                  {
                    *_out << "OK\n";
                  }
                  else
                  {
                    *_out << fc::json::to_pretty_string(result) << "\n";
                  }
                }
                catch( const fc::key_not_found_exception& )
                {
                   elog( " KEY NOT FOUND " );
                   *_out << "key not found \n";
                }
                catch( ... )
                {
                   *_out << "unexpected exception \n";
                }
              }
            }

            string pretty_shorten(const string& str, uint32_t size)
            {
                if (str.size() < size)
                    return str;
                return str.substr(0, size - 3) + "...";
            }

            void print_transfer_summary(const signed_transaction& trx)
            {
                auto trx_rec = _client->get_wallet()->lookup_transaction(trx.id());
                auto pretty = _client->get_wallet()->to_pretty_trx( *trx_rec );
                std::vector<pretty_transaction> list = { pretty };
                print_transaction_history(list);
            }

            void print_unspent_balances(const vector<wallet_balance_record>& balance_recs)
            {
                *_out << std::right;
                *_out << std::setw(18) << "BALANCE";
                *_out << std::right << std::setw(40) << "OWNER";
                *_out << std::right << std::setw(25) << "VOTE";
                *_out << "\n";
                *_out << "-------------------------------------------------------------";
                *_out << "-------------------------------------------------------------\n";
                for( auto balance_rec : balance_recs )
                {
                    *_out << std::setw(18) << balance_rec.balance;
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
                                *_out << std::setw(40) << owner.substr(0, 31) << "...";
                            }
                            else
                            {
                                *_out << std::setw(40) << owner;
                            }

                            auto delegate_id = balance_rec.condition.delegate_id;
                            auto delegate_rec = _client->get_chain()->get_account_record( delegate_id );
                            if( delegate_rec )
                            {
                               string sign = (delegate_id > 0 ? "+" : "-");
                               if (delegate_rec->name.size() > 21)
                               {
                                   *_out << std::setw(25) << (sign + delegate_rec->name.substr(0, 21) + "...");
                               }
                               else
                               {
                                   *_out << std::setw(25) << (sign + delegate_rec->name);
                               }
                               break;
                            }
                            else
                            {
                                   *_out << std::setw(25) << "none";
                            }
                        }
                        default:
                        {
                            FC_ASSERT(!"unimplemented condition type");
                        }
                    } // switch cond type
                    *_out << "\n";
                } // for balance in balances
            }

            void print_contact_account_list(const vector<wallet_account_record> account_records)
            {
                *_out << std::setw( 35 ) << std::left << "NAME (* delegate)";
                *_out << std::setw( 64 ) << "KEY";
                *_out << std::setw( 22 ) << "REGISTERED";
                *_out << std::setw( 15 ) << "TRUST LEVEL";
                *_out << "\n";

                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                    }
                    else
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 34);
                    }

                    *_out << std::setw(64) << string( acct.active_key() );

                    if( acct.id == 0 )
                    {
                       *_out << std::setw( 22 ) << "NO";
                    } 
                    else 
                    {
                        *_out << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }
                    
                    *_out << std::setw( 10) << acct.trust_level;
                    *_out << "\n";
                }
            }


            void print_receive_account_list(const vector<wallet_account_record>& account_records)
            {
                *_out << std::setw( 35 ) << std::left << "NAME (* delegate)";
                *_out << std::setw( 64 ) << "KEY";
                *_out << std::setw( 22 ) << "REGISTERED";
                *_out << std::setw( 15 ) << "TRUST LEVEL";
                *_out << std::setw( 25 ) << "BLOCK PRODUCTION ENABLED";
                *_out << "\n";

                //*_out << fc::json::to_string( account_records ) << "\n";

                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                    }
                    else
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 34);
                    }

                   // auto balance = _client->get_wallet()->get_balances( BTS_ADDRESS_PREFIX, acct.name );
                   // *_out << std::setw(25) << _client->get_chain()->to_pretty_asset(balance[0]);

                    *_out << std::setw(64) << string( acct.active_key() );

                    if (acct.id == 0 ) 
                    { //acct.registration_date == fc::time_point_sec()) {
                        *_out << std::setw( 22 ) << "NO";
                    } 
                    else 
                    {
                        *_out << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                             boost::posix_time::from_time_t(time_t(acct.registration_date.sec_since_epoch())));
                    }

                    *_out << std::setw( 15 ) << acct.trust_level;
                    if (acct.is_delegate())
                        *_out << std::setw( 25 ) << (acct.block_production_enabled ? "YES" : "NO");
                    else
                        *_out << std::setw( 25 ) << "N/A";
                    *_out << "\n";
                }
            }

            void print_registered_account_list(const vector<account_record> account_records, int32_t count )
            {
                *_out << std::setw( 35 ) << std::left << "NAME (* delegate)";
                *_out << std::setw( 64 ) << "KEY";
                *_out << std::setw( 22 ) << "REGISTERED";
                *_out << std::setw( 15 ) << "VOTES FOR";
                *_out << std::setw( 15 ) << "VOTES AGAINST";
                *_out << std::setw( 15 ) << "TRUST LEVEL";

                *_out << "\n";
                auto counter = 0;
                for( auto acct : account_records )
                {
                    if (acct.is_delegate())
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                    }
                    else
                    {
                        *_out << std::setw(35) << pretty_shorten(acct.name, 34);
                    }
                    
                    *_out << std::setw(64) << string( acct.active_key() );
                    *_out << std::setw( 22 ) << boost::posix_time::to_iso_extended_string( 
                                    boost::posix_time::from_time_t( time_t( acct.registration_date.sec_since_epoch() ) ) );

                    if ( acct.is_delegate() )
                    {
                        *_out << std::setw(15) << acct.delegate_info->votes_for;
                        *_out << std::setw(15) << acct.delegate_info->votes_against;
                    }
                    else
                    {
                        *_out << std::setw(15) << "N/A";
                        *_out << std::setw(15) << "N/A";
                    }
                    if ( ! _client->get_wallet()->is_open() )
                    {
                        *_out << "?? (wallet closed)";
                    }
                    else
                    {
                        auto trust = _client->get_wallet()->get_delegate_trust_level( acct.name );
                        *_out << std::setw( 15 ) << trust;
                    }

                    *_out << "\n";

                    /* Count is positive b/c CLI overrides default -1 arg */
                    if (counter >= count)
                    {
                        *_out << "... Use \"blockchain_list_registered_accounts <start_name> <count>\" to see more.\n";
                        return;
                    }
                    counter++;

                }
            }
            void print_wallet_market_order_list( const vector<market_order_status>& order_list )
            {
                if( !_out ) return;

                std::ostream& out = *_out;

                out << std::setw( 40 ) << std::left << "ID";
                out << std::setw( 10 )  << "TYPE";
                out << std::setw( 20 ) << "QUANTITY";
                out << std::setw( 20 ) << "PRICE";
                out << std::setw( 20 ) << "COST";
                out << "\n";
                out <<"-----------------------------------------------------------------------------------------------------------\n";

                for( auto order : order_list )
                {
                   out << std::setw( 40 )  << std::left << variant( order.order.market_index.owner ).as_string(); //order.get_id();
                   out << std::setw( 10  )  << variant( order.get_type() ).as_string();
                   out << std::setw( 20  ) << _client->get_chain()->to_pretty_asset( order.get_quantity() );
                   out << std::setw( 20  ) << _client->get_chain()->to_pretty_price( order.get_price() ); //order.market_index.order_price );
                   out << std::setw( 20  ) << _client->get_chain()->to_pretty_asset( order.get_balance() );
                   out << "\n";
                }
                out << "\n";
            }

            void print_transaction_history(const vector<bts::wallet::pretty_transaction> txs)
            {
                /* Print header */
                if( !_out ) return;

                (*_out) << std::setw(  7 ) << "BLK" << ".";
                (*_out) << std::setw(  5 ) << std::left << "TRX";
                (*_out) << std::setw( 20 ) << "TIMESTAMP";
                (*_out) << std::setw( 20 ) << "FROM";
                (*_out) << std::setw( 20 ) << "TO";
                (*_out) << std::setw( 35 ) << "MEMO";
                (*_out) << std::setw( 20 ) << std::right << "AMOUNT";
                (*_out) << std::setw( 20 ) << "FEE    ";
                (*_out) << std::setw( 12 ) << "ID";
                (*_out) << "\n---------------------------------------------------------------------------------------------------";
                (*_out) <<   "-------------------------------------------------------------------------\n";
                (*_out) << std::right; 
                
                int count = 1;
                for( auto tx : txs )
                {
                    count++;

                    /* Print block and transaction numbers */
                    if (tx.block_num == -1)
                    {
                        (*_out) << std::setw( 13 ) << std::left << "   pending";
                    }
                    else
                    {
                        (*_out) << std::setw( 7 ) << tx.block_num << ".";
                        (*_out) << std::setw( 5 ) << std::left << tx.trx_num;
                    }

                    /* Print timestamp */
                     (*_out) << std::setw( 20 ) << boost::posix_time::to_iso_extended_string( boost::posix_time::from_time_t( tx.received_time ) );

                    // Print "from" account
                     (*_out) << std::setw( 20 ) << pretty_shorten(tx.from_account, 19);
                    
                    // Print "to" account
                     (*_out) << std::setw( 20 ) << pretty_shorten(tx.to_account, 19);

                    // Print "memo" on transaction
                     (*_out) << std::setw( 35 ) << pretty_shorten(tx.memo_message, 34);

                    /* Print amount */
                    {
                        (*_out) << std::right;
                        std::stringstream ss;
                        ss << _client->get_chain()->to_pretty_asset(tx.amount);
                        (*_out) << std::setw( 20 ) << ss.str();
                    }

                    /* Print fee */
                    {
                        (*_out) << std::right;
                        std::stringstream ss;
                        ss << _client->get_chain()->to_pretty_asset( asset(tx.fees,0 ));
                        (*_out) << std::setw( 20 ) << ss.str();
                    }

                    *_out << std::right;
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
                        (*_out) << std::setw(14) << ss.str();
                    }
                    else
                    {
                    }

                    (*_out) << std::right;
                    /* Print transaction ID */
                    (*_out) << std::setw( 16 ) << string(tx.trx_id).substr(0, 8);// << "  " << string(tx.trx_id);

                    (*_out) << std::right << "\n";
                }
            }
            void display_status_message(const std::string& message);
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
    extern "C" int get_character(FILE* stream);
#endif

    cli_impl::cli_impl(const client_ptr& client, std::istream* command_script, std::ostream* output_stream)
    :_client(client)
    ,_rpc_server(client->get_rpc_server())
    ,_quit(false)
    ,show_raw_output(false)
    ,_daemon_mode(false)
    ,nullstream(boost::iostreams::null_sink())
    ,_out(output_stream ? output_stream : &nullstream)
    ,_command_script(command_script)
    {
#ifdef HAVE_READLINE
      //if( &output_stream == &std::cout ) // readline
      {
         cli_impl_instance = this;
         _method_data_is_initialized = false;
         rl_attempted_completion_function = &json_completion_function;
         rl_getc_function = &get_character;
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
        vector<bts::api::method_data> method_data_list = _rpc_server->get_all_method_data();
        for (const bts::api::method_data& method_data : method_data_list)
        {
          _method_data_map[method_data.name] = method_data;
          _method_alias_map[method_data.name] = method_data.name;
          for (const string& alias : method_data.aliases)
            _method_alias_map[alias] = method_data.name;
        }
      }
    }
    extern "C" int get_character(FILE* stream)
    {
      return cli_impl_instance->_cin_thread.async([stream](){ return rl_getc(stream); }).wait();
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

      while (_command_completion_generator_iter != _method_alias_map.end())
      {
        if (_command_completion_generator_iter->first.compare(0, strlen(text), text) != 0)
          break; // no more matches starting with this prefix

        if (_command_completion_generator_iter->second == _command_completion_generator_iter->first) // suppress completing aliases
          return strdup(_command_completion_generator_iter->second.c_str());
        else
          ++_command_completion_generator_iter;  
      }

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
          catch( const bts::rpc::unknown_method& )
          {
            // do nothing
          }
        }
        
        return rl_completion_matches(text, &json_argument_completion_generator_function);
      }
    }

#endif
    void cli_impl::display_status_message(const std::string& message)
    {
      if( !_input_stream || !_out || _daemon_mode ) 
        return;
#ifdef HAVE_READLINE
      char* saved_line = rl_copy_text(0, rl_end);
      char* saved_prompt = strdup(rl_prompt);
      int saved_point = rl_point;
      rl_set_prompt("");
      rl_replace_line("", 0);
      rl_redisplay();
      (*_out) << message << "\n";
      _out->flush();
      rl_set_prompt(saved_prompt);
      rl_replace_line(saved_line, 0);
      rl_point = saved_point;
      rl_redisplay();
      free(saved_line);
      free(saved_prompt);
#else
      // not supported; no way for us to restore the prompt, so just swallow the message
#endif
    }

    void cli_impl::process_commands(std::istream* input_stream)
    {  try {
      FC_ASSERT( input_stream != nullptr );
      _input_stream = input_stream;
      string line = get_line(get_prompt());
      while (_input_stream->good() && !_quit )
      {
        if (!execute_command_line(line))
          break;
        if( !_quit )
          line = get_line( get_prompt() );
      } // while cin.good
      wlog( "process commands exiting" );
    }  FC_CAPTURE_AND_RETHROW() }

  } // end namespace detail

   cli::cli( const client_ptr& client, std::istream* command_script, std::ostream* output_stream)
  :my( new detail::cli_impl(client,command_script,output_stream) )
  {
    my->_self = this;
  }

  void cli::set_input_stream_log(boost::optional<std::ostream&> input_stream_log)
  {
    my->_input_stream_log = input_stream_log;
  } 

  //disable reading from std::cin
  void cli::set_daemon_mode(bool daemon_mode) { my->_daemon_mode = daemon_mode; }
 
  void cli::display_status_message(const std::string& message)
  {
    my->display_status_message(message);
  }
 
  void cli::process_commands(std::istream* input_stream)
  {
    ilog( "starting to process interactive commands" );
    my->process_commands(input_stream);
  }

  cli::~cli()
  {
    try
    {
      wait_till_cli_shutdown();
    }
    catch( const fc::exception& e )
    {
      wlog( "${e}", ("e",e.to_detail_string()) );
    }
  }

  void cli::start() { my->start(); }

  void cli::wait_till_cli_shutdown()
  {
     ilog( "waiting on server to quit" );
     my->_rpc_server->wait_till_rpc_server_shutdown();
  }

  bool cli::execute_command_line( const string& line, std::ostream* output)
  {
    auto old_output = my->_out;
    auto old_input = my->_input_stream;
    bool result = false;
    if( output != &my->nullstream )
    {
        my->_out = output;
        my->_input_stream = nullptr;
    }
    result = my->execute_command_line(line);
    if( output != &my->nullstream)
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
