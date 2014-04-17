#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/cli/cli.hpp>
#include <bts/dns/dns_cli.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_wallet.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>

#include <iostream>

struct config
{
   config():ignore_console(false){}
   bts::rpc::rpc_server::config rpc;
   bool                         ignore_console;
};

FC_REFLECT( config, (rpc)(ignore_console) )


void print_banner();
void configure_logging();
fc::path get_data_dir(int argc, char**argv);
config   load_config( const fc::path& datadir );

int main( int argc, char** argv )
{
   try {
      print_banner();
      configure_logging();

      auto datadir = get_data_dir(argc,argv);
      auto cfg     = load_config(datadir);

      //auto chain   = std::make_shared<bts::blockchain::chain_database>();
      auto chain   = std::make_shared<bts::dns::dns_db>();
      chain->open( datadir / "chain", true );
      chain->set_trustee( bts::blockchain::address( "43cgLS17F2uWJKKFbPoJnnoMSacj" ) );


      //auto wall    = std::make_shared<bts::wallet::wallet>();
      auto wall    = std::make_shared<bts::dns::dns_wallet>();
      wall->set_data_directory( datadir );
      //TODO rescan here?

      auto c = std::make_shared<bts::client::client>();
      c->set_chain( chain );
      c->set_wallet( wall );

      if( fc::exists( "trustee.key" ) )
      {
         auto key = fc::json::from_file( "trustee.key" ).as<fc::ecc::private_key>();
         c->run_trustee(key);
      }

      //auto cli = std::make_shared<bts::cli::cli>( c );
      auto cli = std::make_shared<bts::dns::dns_cli>( c );

      c->add_node( "127.0.0.1:4567" );

      cli->wait();

   } 
   catch ( const fc::exception& e ) 
   {
      wlog( "${e}", ("e", e.to_detail_string() ) );
   }
   return 0;
}



void print_banner()
{
    std::cout<<"================================================================\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=             Welcome to BitShares DNS                         =\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=  This software is in alpha testing and is not suitable for   =\n";
    std::cout<<"=  real monetary transactions or trading.  Use at your own     =\n";
    std::cout<<"=  risk.                                                       =\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=  Type 'help' for usage information.                          =\n";
    std::cout<<"================================================================\n";
}

void configure_logging()
{
   fc::file_appender::config ac;
   ac.filename = "log.txt";
   ac.truncate = false;
   ac.flush    = true;
   fc::logging_config cfg;

   cfg.appenders.push_back(fc::appender_config( "default", "file", fc::variant(ac)));

   fc::logger_config dlc;
   dlc.level = fc::log_level::debug;
   dlc.name = "default";
   dlc.appenders.push_back("default");
   cfg.loggers.push_back(dlc);
   fc::configure_logging( cfg );
}


fc::path get_data_dir( int argc, char** argv )
{ try {
   fc::path datadir;
   if( argc == 1 )
   {
#ifdef WIN32
        datadir =  fc::app_path() / "dotp2p";
#elif defined( __APPLE__ )
        datadir =  fc::app_path() / "dotp2p";
#else
        datadir = fc::app_path() / ".dotp2p";
#endif
   }
   else if( argc == 2 )
   {
        datadir = fc::path( argv[1] );
   }
   else
   {
        std::cerr<<"Usage: "<<argv[0]<<" [DATADIR]\n";
        exit(-2);
   }
   return datadir;

} FC_RETHROW_EXCEPTIONS( warn, "error loading config" ) }



config load_config( const fc::path& datadir )
{ try {
      auto config_file = datadir/"config.json";
      config cfg;
      if( fc::exists( config_file ) )
      {
        cfg = fc::json::from_file( config_file ).as<config>();
      }
      else
      {
         std::cerr<<"creating default config file "<<config_file.generic_string()<<"\n";
         fc::json::save_to_file( cfg, config_file );
      }
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }
