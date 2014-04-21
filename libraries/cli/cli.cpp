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
            bool check_unlock()
            {
               if( _client->get_wallet()->is_locked() )
               {
                  auto password = _self->get_line( "key password: " );
                  try {
                     _client->get_wallet()->unlock_wallet( password );
                  }
                  catch ( ... )
                  {
                     std::cout << "Invalid Password\n";
                     return false;
                  }
               }
               return true;
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
              bts::blockchain::address destination_address = arguments[0].as<bts::blockchain::address>();
              bts::blockchain::asset amount = arguments[1].as<bts::blockchain::asset>();
              std::string comment;
              std::string comment_to;
              if (arguments.size() >= 3)
                comment = arguments[2].as_string();
              if (arguments.size() >= 4)
                comment_to = arguments[3].as_string();
              
              // create_sendtoaddress_transaction takes the same arguments as sendtoaddress
              fc::variant result = execute_command_and_prompt_for_passwords("create_sendtoaddress_transaction", arguments);
              bts::blockchain::signed_transaction transaction = result.as<bts::blockchain::signed_transaction>();
              
              std::string response;
              std::cout << "About to broadcast transaction:\n\n";
              std::cout << _client->get_wallet()->get_tx_info_string(*_client->get_chain(), transaction) << "\n";
              std::cout << "Send this transaction? (Y/n)\n";
              std::cin >> response;

              if (response == "Y")
              {
                result = execute_command_and_prompt_for_passwords("sendtransaction", fc::variants{fc::variant(transaction)});
                bts::blockchain::transaction_id_type transaction_id = result.as<bts::blockchain::transaction_id_type>();
                std::cout << "Transaction sent (id is " << transaction_id << ")\n";
                return result;
              }
              else
              {
                std::cout << "Transaction canceled.\n";
                return fc::variant(false);
              }
            }

            void parse_interactive_command(const std::string& line_to_parse, std::string& command, fc::variants& arguments)
            {
              parse_json_command(line_to_parse, command, arguments);
            }

            fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments)
            {
              if (command == "sendtoaddress")
              {
                // the raw json version sends immediately, in the interactive version we want to 
                // generate the transaction, have the user confirm it, then broadcast it
                interactive_sendtoaddress(command, arguments);
              }
              else
                return execute_command_and_prompt_for_passwords(command, arguments);
            }
            void format_and_print_result(const std::string& command, const fc::variant& result)
            {
              if (command == "sendtoaddress")
              {
                // sendtoaddress does its own output formatting
              }
              else if( command == "listrecvaddresses" )
              {
                std::cout << std::setw( 33 ) << std::left << "address" << " : " << "account" << "\n";
                std::cout << "--------------------------------------------------------------------------------\n";
                auto addrs = _client->get_wallet()->get_recv_addresses();
                for( auto addr : addrs )
                  std::cout << std::setw( 33 ) << std::left << std::string(addr.first) << " : " << addr.second << "\n";
              }
              else
                std::cout << fc::json::to_pretty_string(result) << "\n";
            }

            void process_commands()
            {
              std::string line = _self->get_line();
              while (std::cin.good())
              {
                std::string trimmed_line_to_parse(boost::algorithm::trim_copy(line));
                if (!trimmed_line_to_parse.empty())
                {
                  std::string command;
                  fc::variants arguments;
                  _self->parse_interactive_command(trimmed_line_to_parse, command, arguments);

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
        std::cout << "You will be asked to provide two passphrase, the first passphrase \n";
        std::cout << "encrypts the entire contents of your wallet on disk.  The second\n ";
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

        std::cout << "Please set a passphrase for encrypting your private keys: \n";
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
      try {
        wait();
      }
      catch ( const fc::exception& e )
      {
         wlog( "${e}", ("e",e.to_detail_string()) );
      }
   }

   void cli::print_help()
   {
      std::cout<<"Commands\n";
      std::cout<<"-------------------------------------------------------------\n";
      std::cout<<"help - print available commands\n";
      std::cout<<"unlock - asks for password to decrypt private keys \n";
      std::cout<<"listrecvaddresses\n";
      std::cout<<"getnewaddress [ACCOUNT] \n";
      std::cout<<"sendtoaddress ADDRESS AMOUNT [MEMO] \n";
      std::cout<<"getbalance [ACCOUNT] [MIN_CONF] \n";
      std::cout<<"listtransactions [COUNT]\n";
      std::cout<<"rescan [BLOCK_NUM=0]\n";
      std::cout<<"import_bitcoin_wallet WALLET_DAT\n";
      std::cout<<"import_private_key    HEX_PRIV_KEY\n";
      std::cout<<"listunspent\n";
      std::cout<<"quit - exit cleanly\n";
      std::cout<<"-------------------------------------------------------------\n";
   } // print_help
   
#if 0
   void cli::process_command( const std::string& cmd, const std::string& args )
   {
       const bts::blockchain::chain_database_ptr db = client()->get_chain();
       const bts::wallet::wallet_ptr wallet = client()->get_wallet();

       std::stringstream ss(args);

       if( cmd == "help" ) print_help();
       else if( cmd == "login" )
       {

       }
       else if( cmd == "getnewaddress" )
       {
          if( my->check_unlock() )
          {
             std::string account;
             ss >> account;

             auto addr = wallet->new_recv_address( account );
             std::cout << std::string( addr ) << "\n";
          }
       }
       else if( cmd == "listunspent" )
       {
          wallet->dump_utxo_set();
       }
       else if( cmd == "listrecvaddresses" )
       {
           auto addrs = wallet->get_recv_addresses();
           for( auto addr : addrs )
              std::cout << std::setw( 30 ) << std::left << std::string(addr.first) << " : " << addr.second << "\n";
       }
       else if( cmd == "import" )
       {

       }
       else if( cmd == "listtransactions" )
       {
          uint32_t count = 0;
          ss >> count;
          list_transactions( count );
       }
       else if( cmd == "rescan" )
       {
          uint32_t block_num = 0;
          ss >> block_num;
          wallet->scan_chain( *db, block_num, [](uint32_t cur, uint32_t last, uint32_t trx, uint32_t last_trx)
                                                 {
                                                     std::cout << "scanning transaction " <<  cur << "." << trx <<"  of " << last << "." << last_trx << "         \r";
                                                 });
          std::cout << "\ndone scanning block chain\n";
       }
       else if( cmd == "expoet" )
       {

       }
       else if( cmd == "import_bitcoin_wallet" )
       {
          if( my->check_unlock() )
          {
             // TODO: Enforce # of arguments
             std::string wallet_dat;
             ss >> wallet_dat;

             auto password  = get_line("bitcoin wallet password: ");
             wallet->import_bitcoin_wallet( wallet_dat, password );
             wallet->save();
          }
       }
       else if( cmd == "quit" )
       {
          FC_THROW_EXCEPTION( canceled_exception, "quit command issued" );
       }
       else
       {
          std::cout<<"Unknown command '"<<cmd<<"'\n\n";
          print_help();
       }
   }
#endif
   void cli::list_transactions( uint32_t count )
   {
       /* dump the transactions from the wallet, which needs the chain db */
       client()->get_wallet()->dump_txs(*(client()->get_chain()), count);
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

   bool cli::check_unlock()
   {
       return my->check_unlock();
   }

  void cli::parse_interactive_command(const std::string& line_to_parse, std::string& command, fc::variants& arguments)
  {
    my->parse_interactive_command(line_to_parse, command, arguments);
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
