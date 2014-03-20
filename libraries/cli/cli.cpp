#include <bts/cli/cli.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>
#include <sstream>

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
            bts::cli::cli*   _self;
            fc::thread*      _main_thread;
            fc::thread       _cin_thread;
            fc::future<void> _cin_complete;

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

            void process_commands()
            {
                  _self->process_command( "login", "" );
                  auto line = _self->get_line();
                  while( std::cin.good() )
                  {
                     std::stringstream ss(line);
                     std::string command, args;

                     ss >> command;
                     std::getline( ss, args );

                     try {
                       _self->process_command( command, args ); 
                     } 
                     catch ( const fc::canceled_exception& c )
                     {
                        return;
                     }
                     catch ( const fc::exception& e )
                     {
                        std::cout << e.to_detail_string() << "\n";
                     }
                     line = _self->get_line();
                  }
            }
      };
   }

   cli::cli( const client_ptr& c )
   :my( new detail::cli_impl() )
   {
      my->_client      = c;
      my->_self        = this;
      my->_main_thread = &fc::thread::current();


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
      std::cout<<"transfer AMOUNT UNIT to ADDRESS\n";
      std::cout<<"import_bitcoin_wallet WALLET_DAT\n";
      std::cout<<"import_private_key    WALLET_DAT\n";
      std::cout<<"quit - exit cleanly\n";
      std::cout<<"-------------------------------------------------------------\n";
   } // print_help

   void cli::process_command( const std::string& cmd, const std::string& args )
   {
       if( cmd == "help" ) print_help();
       else if( cmd == "login" )
       {
          auto wallet_dat = my->_client->get_wallet()->get_wallet_file();
          if( fc::exists( wallet_dat ) )
          {
             std::cout << "Login\n";
             auto pass = get_line("password: ");
             my->_client->get_wallet()->open( wallet_dat, pass );
             std::cout << "Login Successful.\n";
             return;
          }
          else
          {
             std::cout << "Creating wallet "<< wallet_dat.generic_string() << "\n";
             std::cout << "You will be asked to provide two passwords, the first password \n";
             std::cout << "encrypts the entire contents of your wallet on disk.  The second\n ";
             std::cout << "passowrd will only encrypt your private keys.\n\n";

             std::cout << "Please set a password for encrypting your wallet: \n";
             std::string pass1, pass2;
             pass1  = get_line("password: ");
             while( pass1 != pass2 )
             {
                pass2 = get_line("password (again): ");
                if( pass2 != pass1 )
                {
                  std::cout << "Your passwords did not match, please try again.\n";
                  pass2 = std::string();
                  pass1 = get_line("password: ");
                }
             }
             if( pass1 == std::string() )
             {
                std::cout << "No password provided, your wallet will be stored unencrypted.\n";
             }

             std::cout << "Please set a password for encrypting your private keys: \n";
             std::string keypass1, keypass2;
             bool retry = false;
             keypass1  = get_line("key password: ");
             while( keypass1 != keypass2 )
             {
                if( keypass1.size() > 8 )
                   keypass2 = get_line("key password (again): ");
                else
                {
                   std::cout << "Your key password must be more than 8 characters.\n";
                   retry = true;
                }
                if( keypass2 != keypass1 )
                {
                   std::cout << "Your passwords did not match.\n";
                   retry = true;
                }
                if( retry )
                {
                  retry = false;
                  std::cout << "Please try again.\n";
                  keypass2 = std::string();
                  keypass1 = get_line("password: ");
                }
             }
             if( keypass1 == std::string() )
             {
                std::cout << "No password provided, your wallet will be stored unencrypted.\n";
             }
            
             my->_client->get_wallet()->create( wallet_dat, pass1, keypass1 );
             std::cout << "Wallet created.\n";
          }
       }
       else if( cmd == "transfer" )
       {
       }
       else if( cmd == "import" )
       {

       }
       else if( cmd == "export" )
       {

       }
       else if( cmd == "balance" )
       {

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
   
   void cli::wait()
   {
       my->_cin_complete.wait();
   }

   std::string cli::get_line( const std::string& prompt )
   {
       return my->_cin_thread.async( [=](){ return my->get_line( prompt ); } ).wait();
   }





} } // bts::cli
