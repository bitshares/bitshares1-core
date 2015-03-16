#include <bts/blockchain/withdraw_types.hpp>
#include <bts/cli/cli.hpp>
#include <bts/cli/pretty.hpp>
#include <bts/cli/print_result.hpp>
#include <bts/rpc/exceptions.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/url.hpp>

#include <fc/io/buffered_iostream.hpp>
#include <fc/io/console.hpp>
#include <fc/io/json.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/optional.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/algorithm/min_element.hpp>

#include <iomanip>
#include <iostream>

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

  class buffered_istream_with_eot_hack : public virtual fc::buffered_istream
  {
  public:
    buffered_istream_with_eot_hack(fc::istream_ptr is) :
      fc::buffered_istream(is) {}
    buffered_istream_with_eot_hack(fc::buffered_istream&& o) :
      fc::buffered_istream(std::move(o)) {}

    std::size_t readsome(char* buf, std::size_t len) override
    {
      std::size_t bytes_read = fc::buffered_istream::readsome(buf, len);
      assert(bytes_read > 0);
      if (buf[bytes_read - 1] == 0x04)
        --bytes_read;
      if (bytes_read == 0)
        FC_THROW_EXCEPTION(fc::eof_exception, "");
      return bytes_read;
    }

    size_t readsome(const std::shared_ptr<char>& buf, size_t len, size_t offset) override
    {
      std::size_t bytes_read = fc::buffered_istream::readsome(buf, len, offset);
      assert(bytes_read > 0);
      if (buf.get()[offset + bytes_read - 1] == 0x04)
        --bytes_read;
      if (bytes_read == 0)
        FC_THROW_EXCEPTION(fc::eof_exception, "");
      return bytes_read;
    }

    char peek() const override
    {
      char c = fc::buffered_istream::peek();
      if (c == 0x04)
        FC_THROW_EXCEPTION(fc::eof_exception, "");
      return c;
    }

    char get() override
    {
      char c = fc::buffered_istream::get();
      if (c == 0x04)
        FC_THROW_EXCEPTION(fc::eof_exception, "");
      return c;
    }
  };


  FC_DECLARE_EXCEPTION( cli_exception, 11000, "CLI Error" )
  FC_DECLARE_DERIVED_EXCEPTION( abort_cli_command, bts::cli::cli_exception, 11001, "command aborted by user" );
  FC_DECLARE_DERIVED_EXCEPTION( exit_cli_command, bts::cli::cli_exception, 11002, "exit CLI client requested by user" );

  string get_line(
      std::istream* input_stream,
      std::ostream* out,
      const string& prompt,          // = CLI_PROMPT_SUFFIX
      bool no_echo,                  // = false
      fc::thread* cin_thread,        // = nullptr
      bool saved_out,                // = false
      std::ostream* input_stream_log // = nullptr
      )
  {
          if( input_stream == nullptr )
             FC_CAPTURE_AND_THROW( bts::cli::exit_cli_command ); //_input_stream != nullptr );

          //FC_ASSERT( _self->is_interactive() );
          string line;
          if ( no_echo )
          {
              *out << prompt;
              // there is no need to add input to history when echo is off, so both Windows and Unix implementations are same
              fc::set_console_echo(false);
              sync_call( cin_thread, [&](){ std::getline( *input_stream, line ); }, "getline");
              fc::set_console_echo(true);
              *out << std::endl;
          }
          else
          {
          #ifdef HAVE_READLINE
            if (input_stream == &std::cin)
            {
              char* line_read = nullptr;
              out->flush(); //readline doesn't use cin, so we must manually flush _out
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
              *out <<prompt;
              sync_call( cin_thread, [&](){ std::getline( *input_stream, line ); }, "getline");
            }
          #else
            *out <<prompt;
            sync_call( cin_thread, [&](){ std::getline( *input_stream, line ); }, "getline");
          #endif
          if ( input_stream_log != nullptr )
            {
            out->flush();
            if (saved_out)
              *input_stream_log << CLI_PROMPT_SUFFIX;
            *input_stream_log << line << std::endl;
            }
          }

          boost::trim(line);
          return line;
    }


  namespace detail
  {
      class cli_impl
      {
         public:
            bts::client::client*                            _client;
            rpc_server_ptr                                  _rpc_server;
            bts::cli::cli*                                  _self;
            fc::thread                                      _cin_thread;

            bool                                            _quit;
            bool                                            show_raw_output;
            bool                                            _daemon_mode;

            boost::iostreams::stream< boost::iostreams::null_sink > nullstream;

            std::ostream*                  _saved_out;
            std::ostream*                  _out;   //cout | log_stream | tee(cout,log_stream) | null_stream
            std::istream*                  _command_script;
            std::istream*                  _input_stream;
            boost::optional<std::ostream&> _input_stream_log;

            print_result                   _print_result;

            cli_impl(bts::client::client* client, std::istream* command_script, std::ostream* output_stream);

            void process_commands(std::istream* input_stream);

            void start()
            {
                try
                {
                  if (_command_script)
                    process_commands(_command_script);
                  if (_daemon_mode)
                  {
                    _rpc_server->wait_till_rpc_server_shutdown();
                    return;
                  }
                  else if (!_quit)
                    process_commands(&std::cin);
                }
                catch ( const fc::exception& e)
                {
                    *_out << "\nshutting down\n";
                    elog( "${e}", ("e",e.to_detail_string() ) );
                }
                if( _rpc_server )
                    _rpc_server->shutdown_rpc_server();
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

              buffered_istream_with_eot_hack buffered_argument_stream(argument_stream);

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
              catch( const bts::cli::abort_cli_command&)
              {
                throw;
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
                catch( const bts::cli::abort_cli_command&)
                {
                  throw;
                }
                catch( const bts::cli::exit_cli_command&)
                {
                  throw;
                }
                catch( const fc::exception& e )
                {
                  if( FILTER_OUTPUT_FOR_TESTS )
                    *_out << "Command failed with exception: " << e.to_string() << "\n";
                  else
                    *_out << e.to_detail_string() << "\n";
                }
              }
            } //parse_and_execute_interactive_command

            bool execute_command_line(const string& line)
            { try {
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
                catch (const bts::cli::exit_cli_command&)
                {
                  return false;
                }
                catch( const bts::cli::abort_cli_command& )
                {
                  *_out << "Command aborted\n";
                }
              } //end if command line not empty
              return true;
            } FC_RETHROW_EXCEPTIONS( warn, "", ("command",line) ) }

            string get_line( const string& prompt = CLI_PROMPT_SUFFIX, bool no_echo = false)
            {
                  if( _quit ) return std::string();
                  return bts::cli::get_line(
                      _input_stream,
                      _out,
                      prompt,
                      no_echo,
                      &_cin_thread,
                      _saved_out,
                      _input_stream_log ? &*_input_stream_log : nullptr
                      );
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
                ASSERT_TASK_NOT_PREEMPTED();
                return _self->parse_unrecognized_interactive_command(argument_stream, command);
              }
            }

            fc::variants parse_recognized_interactive_command( fc::buffered_istream& argument_stream,
                                                               const bts::api::method_data& method_data)
            { try {
              fc::variants arguments;
              for( unsigned i = 0; i < method_data.parameters.size(); ++i )
              {
                bool parse_argument_threw_eof = false;
                try
                {
                  arguments.push_back(_self->parse_argument_of_known_type(argument_stream, method_data, i));
                }
                catch( const fc::eof_exception&)
                {
                  parse_argument_threw_eof = true;
                  // handle below
                } //end catch eof_exception
                catch( fc::parse_error_exception& e )
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", i + 1)("command", method_data.name)("detail", e.get_log()));
                }

                if (parse_argument_threw_eof)
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
                      buffered_istream_with_eot_hack buffered_argument_stream(prompt_argument_stream);
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
                } // end if (parse_argument_threw_eof)


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

                if( ! bts::blockchain::address::is_valid(address_string) )
                {
                  FC_THROW_EXCEPTION(fc::parse_error_exception, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name));
                }
                return fc::variant( bts::blockchain::address(address_string) );
              }
              else if (this_parameter.type == "string" ||
                       this_parameter.type == "std::string" ||
                       this_parameter.type == "wallet_name" ||
                       this_parameter.type == "optional_wallet_name" ||
                       this_parameter.type == "wif_private_key" ||
                       this_parameter.type == "receive_account_name" ||
                       this_parameter.type == "sending_account_name" ||
                       this_parameter.type == "asset_symbol" ||
                       this_parameter.type == "name" ||
                       this_parameter.type == "keyhoteeid" ||
                       this_parameter.type == "account_name" ||
                       this_parameter.type == "new_account_name" ||
                       this_parameter.type == "method_name" ||
                       this_parameter.type == "filename" ||
                       this_parameter.type == "public_key" ||
                       this_parameter.type == "order_id" ||
                       this_parameter.type == "account_key_type" ||
                       this_parameter.type == "vote_strategy" ||
                       this_parameter.type == "transaction_id" ||
                       this_parameter.type == "operation_type" ||
                       this_parameter.type == "withdraw_condition_type" ||
                       this_parameter.type == "wallet_record_type" ||
                       this_parameter.type == "asset_flag_enum" ||
                       this_parameter.type == "passphrase")
              {
                string result;

                while( isspace(argument_stream.peek()) )
                  argument_stream.get();

                if (argument_stream.peek() == '\'' || argument_stream.peek() == '"')
                {
                  try {
                    char delimiter = argument_stream.get();
                    char next = argument_stream.get();
                    while (next != delimiter)
                    {
                      result += next;
                      next = argument_stream.get();
                    }
                  }
                  catch( fc::exception& e )
                  {
                    FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                         ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                  }
                }
                else
                {
                  try {
                    while (!isspace(argument_stream.peek()) && isprint(argument_stream.peek()))
                      result += argument_stream.get();
                  } catch (fc::eof_exception) {}
                  if (result.empty())
                    FC_THROW_EXCEPTION(fc::eof_exception, "EOF when parsing argument");
                }

                return result;
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
              if( command == "quit" || command == "stop" || command == "exit" )
              {
                _quit = true;
                FC_THROW_EXCEPTION( bts::cli::exit_cli_command, "quit command issued" );
              }

              return execute_command( command, arguments );
            }

            fc::variant execute_command(const string& command, const fc::variants& arguments)
            { try {
              while (true)
              {
                // try to execute the method.  If the method needs the wallet to be
                // unlocked, it will throw an exception, and we'll attempt to
                // unlock it and then retry the command
                bool wallet_open_needed = false;
                bool wallet_lock_needed = false;
                try
                {
                  return _rpc_server->direct_invoke_method(command, arguments);
                }
                catch( const rpc_wallet_open_needed_exception& )
                {
                  wallet_open_needed = true;
                }
                catch( const rpc_wallet_unlock_needed_exception& )
                {
                  wallet_lock_needed = true;
                }

                if (wallet_open_needed)
                  interactive_open_wallet();
                else if (wallet_lock_needed)
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
                    if( input.empty() )
                      FC_THROW_EXCEPTION(bts::cli::abort_cli_command, "input aborted");

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
              FC_ASSERT(_client->get_wallet()->is_enabled(), "Wallet is not enabled in this client!");

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
                catch( const bts::cli::abort_cli_command& )
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
                catch( const bts::cli::abort_cli_command& )
                {
                }
              }
              else if (choice == "q")
              {
                FC_THROW_EXCEPTION(bts::cli::abort_cli_command, "");
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

              if( !_out ) return;

              _print_result.format_and_print( method_name, arguments, result, _client, *_out );

              *_out << std::right; /* Ensure default alignment is restored */
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

    cli_impl::cli_impl(bts::client::client* client, std::istream* command_script, std::ostream* output_stream)
    :_client(client)
    ,_rpc_server(client->get_rpc_server()),
    _cin_thread("stdin_reader")
    ,_quit(false)
    ,show_raw_output(false)
    ,_daemon_mode(false)
    ,nullstream(boost::iostreams::null_sink())
    , _saved_out(nullptr)
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
        for( const auto& method_data : method_data_list )
        {
          _method_data_map[method_data.name] = method_data;
          _method_alias_map[method_data.name] = method_data.name;
          for( const auto& alias : method_data.aliases )
            _method_alias_map[alias] = method_data.name;
        }
      }
    }
    extern "C" int get_character(FILE* stream)
    {
      return cli_impl_instance->_cin_thread.async([stream](){ return rl_getc(stream); }, "rl_getc").wait();
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
      if (rl_prompt)
      {
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
      }
      else
      {
        (*_out) << message << "\n";
        // it's not clear what state we're in if rl_prompt is null, but we've had reports
        // of crashes.  Just swallow the message and avoid crashing.
      }
#else
      // not supported; no way for us to restore the prompt, so just swallow the message
#endif
    }

    void cli_impl::process_commands(std::istream* input_stream)
    {  try {
      FC_ASSERT( input_stream != nullptr );
      _input_stream = input_stream;
      //force flushing to console and log file whenever input is read
      _input_stream->tie( _out );
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

   cli::cli( bts::client::client* client, std::istream* command_script, std::ostream* output_stream)
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
    if (my)
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
     ilog( "waiting on server to quit" );
     my->_rpc_server->shutdown_rpc_server();
     my->_rpc_server->wait_till_rpc_server_shutdown();
     ilog( "rpc server shut down" );
    }
    catch( const fc::exception& e )
    {
      wlog( "${e}", ("e",e.to_detail_string()) );
    }
  }

  void cli::start() { my->start(); }

  void cli::enable_output(bool enable_output)
  {
    if (!enable_output)
    { //save off original output and set output to nullstream
      my->_saved_out = my->_out;
      my->_out = &my->nullstream;
    }
    else
    { //can only enable output if it was previously disabled
      if (my->_saved_out)
      {
        my->_out = my->_saved_out;
        my->_saved_out = nullptr;
      }
    }

  }

  void cli::filter_output_for_tests(bool enable_flag)
  {
    FILTER_OUTPUT_FOR_TESTS = enable_flag;
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
