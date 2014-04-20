#include <boost/program_options.hpp>

#include <bts/net/chain_server.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/crypto/elliptic.hpp>
#include <iostream>

int main( int argc, char** argv )
{
   // parse command-line options
   boost::program_options::options_description option_config("Allowed options");
   option_config.add_options()("help", "display this help message")
                              ("trustee-address", boost::program_options::value<std::string>(), "trust the given BTS address to generate blocks");
   boost::program_options::variables_map option_variables;
   try
   {
     boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
       options(option_config).run(), option_variables);
     boost::program_options::notify(option_variables);
   }
   catch (boost::program_options::error&)
   {
     std::cerr << "Error parsing command-line options\n\n";
     std::cerr << option_config << "\n";
     return 1;
   }

   if (option_variables.count("help"))
   {
     std::cout << option_config << "\n";
     return 0;
   }

   try {
       fc::configure_logging( fc::logging_config::default_config() );

       bts::net::chain_server cserv;
       bts::net::chain_server::config cfg;
       cfg.port = 4569;
       cserv.configure(cfg);

       if (option_variables.count("trustee-address"))
         cserv.get_chain().set_trustee( bts::blockchain::address(option_variables["trustee-address"].as<std::string>()) );
       else
         cserv.get_chain().set_trustee( bts::blockchain::address( "43cgLS17F2uWJKKFbPoJnnoMSacj" ) );

       ilog( "sleep..." );
       fc::usleep( fc::seconds( 60*60*24*365 ) );
   } 
   catch ( const fc::exception& e )
   {
     elog( "${e}", ("e",e.to_detail_string() ) );
     return -1;
   }
   ilog( "Exiting normally" );
   return 0;
}
