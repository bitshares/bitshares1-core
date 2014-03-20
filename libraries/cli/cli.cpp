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
