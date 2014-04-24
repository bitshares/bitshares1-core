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

#ifndef WIN32
#include <readline/readline.h>
#include <readline/history.h>
#endif //!WIN32

namespace bts { namespace cli {

  namespace detail
  {
      class cli_impl
      {
         public:
            client_ptr       _client;
            bts::rpc::rpc_server_ptr   _rpc_server;
            bts::cli::cli*   _self;
            fc::thread*      _main_thread;
            fc::thread       _cin_thread;
            fc::future<void> _cin_complete;

            cli_impl( const client_ptr& client, const bts::rpc::rpc_server_ptr& rpc_server ) :
              _client(client),
              _rpc_server(rpc_server)
            {}

            void create_wallet_if_missing();

            std::string get_line( const std::string& prompt = ">>> " )
            {
                  std::string line;
                  #ifndef WIN32
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
                  #endif ///WIN32
                  return line;
            }

            void interactive_open_wallet()
            {
              if( !_client->get_wallet()->is_open() )
              {
                try
                {
                  // try to open without a password first
                  _rpc_server->direct_invoke_method("openwallet", fc::variants());
                  return;
                }
                catch (bts::rpc::rpc_wallet_passphrase_incorrect_exception&)
                {
                }

                while (1)
                {
                  std::string password = _self->get_line("wallet passphrase: ");
                  if (password.empty())
                    FC_THROW_EXCEPTION(canceled_exception, "User gave up entering a password");
                  try
                  {
                    fc::variants arguments{password};
                    _rpc_server->direct_invoke_method("openwallet", arguments);
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
                  std::string password = _self->get_line("spending passphrase: ");
                  if (password.empty())
                    FC_THROW_EXCEPTION(canceled_exception, "User gave up entering a password");
                  try
                  {
                    fc::variants arguments{password};
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

            fc::variant execute_command_and_prompt_for_passwords(const std::string& command, const fc::variants& arguments)
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
              fc::variant result = execute_command_and_prompt_for_passwords("_create_sendtoaddress_transaction", arguments);
              bts::blockchain::signed_transaction transaction = result.as<bts::blockchain::signed_transaction>();
              
              std::string response;
              std::cout << "About to broadcast transaction:\n\n";
              std::cout << _client->get_wallet()->get_transaction_info_string(*_client->get_chain(), transaction) << "\n";
              std::cout << "Send this transaction? (Y/n)\n";
              std::cin >> response;

              if (response == "Y")
              {
                result = execute_command_and_prompt_for_passwords("sendtransaction", fc::variants{fc::variant(transaction)});
                bts::blockchain::transaction_id_type transaction_id = result.as<bts::blockchain::transaction_id_type>();
                std::cout << "Transaction sent (id is " << (std::string)transaction_id << ")\n";
                return result;
              }
              else
              {
                std::cout << "Transaction canceled.\n";
                return fc::variant(false);
              }
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
                    std::cout << "scanning transaction " <<  cur << "." << trx <<"  of " << last << "." << last_trx << "         \r";
                });              
              return fc::variant();
            }

            fc::variant parse_argument_of_known_type(fc::buffered_istream& argument_stream, const bts::rpc::rpc_server::method_data& method_data, unsigned parameter_index)
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
                catch (fc::bad_cast_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }
                catch (fc::parse_error_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }                
                return fc::variant(bts::blockchain::asset(amount_as_int));
              }
              else if (this_parameter.type == "address")
              {
                // allow addresses to be un-quoted
                while (isspace(argument_stream.peek()))
                  argument_stream.get();
                fc::stringstream address_stream;
                try
                {
                  while (!isspace(argument_stream.peek()))
                    address_stream.put(argument_stream.get());
                }
                catch (fc::eof_exception&)
                {
                }
                std::string address_string = address_stream.str();

                try
                {
                  bts::blockchain::address::is_valid(address_string);
                }
                catch (fc::exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                } 
                return fc::variant(bts::blockchain::address(address_string));
              }
              else
              {
                // assume it's raw JSON
                try
                {
                  return fc::json::from_stream(argument_stream);
                }
                catch (fc::parse_error_exception& e)
                {
                  FC_RETHROW_EXCEPTION(e, error, "Error parsing argument ${argument_number} of command \"${command}\": ${detail}",
                                        ("argument_number", parameter_index + 1)("command", method_data.name)("detail", e.get_log()));
                }                
              }
            }

            fc::variants parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream, 
                                                                const std::string& command)
            {
              // quit isn't registered with the RPC server
              if (command == "quit")
                return fc::variants();
              FC_THROW_EXCEPTION(key_not_found_exception, "Unknown command \"${command}\".", ("command", command));
            }

            fc::variants parse_recognized_interactive_command(fc::buffered_istream& argument_stream, const bts::rpc::rpc_server::method_data& method_data)
            {
              fc::variants arguments;
              for (unsigned i = 0; i < method_data.parameters.size(); ++i)
              {
                try
                {
                  arguments.push_back(_self->parse_argument_of_known_type(argument_stream, method_data, i));
                }
                catch (fc::eof_exception&)
                {
                  if (!method_data.parameters[i].required)
                    return arguments;
                  else
                    FC_THROW("Missing argument ${argument_number} of command \"${command}\"",
                             ("argument_number", i + 1)("command", method_data.name));
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
                return interactive_rescan(command, arguments);
              else if(command == "quit")
              {
                FC_THROW_EXCEPTION(canceled_exception, "quit command issued");
              }
              else
              {
                return execute_command_and_prompt_for_passwords(command, arguments);
              }
            }
            void format_and_print_result(const std::string& command, const fc::variant& result)
            {
              if (command == "sendtoaddress")
              {
                // sendtoaddress does its own output formatting
              }
              else if(command == "listrecvaddresses")
              {
                std::cout << std::setw( 33 ) << std::left << "address" << " : " << "account" << "\n";
                std::cout << "--------------------------------------------------------------------------------\n";
                auto addrs = _client->get_wallet()->get_receive_addresses();
                for( auto addr : addrs )
                  std::cout << std::setw( 33 ) << std::left << std::string(addr.first) << " : " << addr.second << "\n";
              }
              else if (command == "help")
              {
                std::vector<std::vector<std::string> > help_strings = result.as<std::vector<std::vector<std::string> > >();
                for (const std::vector<std::string>& command_info : help_strings)
                {
                  std::cout << std::setw(35) << std::left << 
                            (command_info[0] + " " + command_info[1]) << "   " << command_info[2] << "\n";
                }
              }
              else if (command == "rescan")
                std::cout << "\ndone scanning block chain\n";
              else
              {
                // there was no custom handler for this particular command, see if the return type
                // is one we know how to pretty-print
                std::string result_type;
                try
                {
                  const bts::rpc::rpc_server::method_data& method_data = _rpc_server->get_method_data(command);
                  result_type = method_data.return_type;
                }
                catch (fc::key_not_found_exception&)
                {
                }
                if (result_type == "asset")
                  std::cout << (std::string)result.as<bts::blockchain::asset>() << "\n";
                else if (result_type == "address")
                  std::cout << (std::string)result.as<bts::blockchain::address>() << "\n";
                else
                  std::cout << fc::json::to_pretty_string(result) << "\n";
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
      auto wallet_dat = _client->get_wallet()->get_wallet_file();
      if( !fc::exists( wallet_dat ) )
      {
        std::cout << "Creating wallet "<< wallet_dat.generic_string() << "\n";
        std::cout << "You will be asked to provide two passphrase, the first passphrase\n";
        std::cout << "encrypts the entire contents of your wallet on disk.  The second\n";
        std::cout << "passphrase will only encrypt your private keys.\n\n";

        std::cout << "Please set a passphrase for encrypting your wallet: \n";
        std::string pass1, pass2;
        pass1  = get_line("wallet passphrase: ");
        while( pass1 != pass2 )
        {
          pass2 = get_line("walletpassword (again): ");
          if( pass2 != pass1 )
          {
            std::cout << "Your passphrases did not match, please try again.\n";
            pass2 = std::string();
            pass1 = get_line("wallet passphrase: ");
          }
        }
        if( pass1 == std::string() )
        {
          std::cout << "No passphrase provided, your wallet will be stored unencrypted.\n";
        }

        std::cout << "\nPlease set a passphrase for encrypting your private keys: \n";
        std::string keypass1, keypass2;
        bool retry = false;
        keypass1  = get_line("spending passphrase: ");
        while( keypass1 != keypass2 )
        {
          if( keypass1.size() > 8 )
              keypass2 = get_line("spending passphrase (again): ");
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
            keypass1 = get_line("spending passphrase: ");
          }
        }
        if( keypass1 == std::string() )
        {
          std::cout << "No passphrase provided, your wallet will be stored unencrypted.\n";
        }

        _client->get_wallet()->create( wallet_dat, pass1, keypass1 );
        std::cout << "Wallet created.\n";
      }
    }
  } // end namespace detail

  cli::cli( const client_ptr& client, const bts::rpc::rpc_server_ptr& rpc_server ) :
    my( new detail::cli_impl(client, rpc_server) )
  {
    my->_self        = this;
    my->_main_thread = &fc::thread::current();

    my->create_wallet_if_missing();

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
#if 0
   void cli::process_command( const std::string& cmd, const std::string& args )
   {
       else if( cmd == "listunspent" )
       {
          wallet->dump_unspent_outputs();
       }
       else if( cmd == "listtransactions" )
       {
          uint32_t count = 0;
          ss >> count;
          list_transactions( count );
       }
   }
   void cli::list_transactions( uint32_t count )
   {
       /* dump the transactions from the wallet, which needs the chain db */
       client()->get_wallet()->dump_txs(*(client()->get_chain()), count);
   }
#endif
   void cli::list_delegates( uint32_t count )
   {
        auto delegates = client()->get_chain()->get_delegates( count );

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

   std::string cli::get_line( const std::string& prompt )
   {
       return my->_cin_thread.async( [=](){ return my->get_line( prompt ); } ).wait();
   }

   client_ptr cli::client()
   {
       return my->_client;
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
