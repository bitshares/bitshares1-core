#include <boost/program_options.hpp>

#include <bts/client/client.hpp>
#include <bts/net/upnp.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/cli/cli.hpp>
#include <bts/utilities/git_revision.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/git_revision.hpp>

#include <iostream>
#include <iomanip>

struct config
{
   config():ignore_console(false){}
   bts::rpc::rpc_server::config rpc;
   bool                         ignore_console;
};

FC_REFLECT( config, (rpc)(ignore_console) )


void print_banner();
void configure_logging(const fc::path&);
fc::path get_data_dir(const boost::program_options::variables_map& option_variables);
config   load_config( const fc::path& datadir );
bts::blockchain::chain_database_ptr load_and_configure_chain_database(const fc::path& datadir,
                                                                      const boost::program_options::variables_map& option_variables);
bts::client::client* _global_client = nullptr;


int main( int argc, char** argv )
{
   // parse command-line options
   boost::program_options::options_description option_config("Allowed options");
   option_config.add_options()("data-dir", boost::program_options::value<std::string>(), "configuration data directory")
                              ("help", "display this help message")
                              ("p2p-port", boost::program_options::value<uint16_t>()->default_value(5678), "set port to listen on")
                              ("upnp", boost::program_options::value<bool>()->default_value(true), "Enable UPNP")
                              ("connect-to", boost::program_options::value<std::vector<std::string> >(), "set remote host to connect to")
                              ("server", "enable JSON-RPC server")
                              ("daemon", "run in daemon mode with no CLI console")
                              ("rpcuser", boost::program_options::value<std::string>(), "username for JSON-RPC")
                              ("rpcpassword", boost::program_options::value<std::string>(), "password for JSON-RPC")
                              ("rpcport", boost::program_options::value<uint16_t>()->default_value(5679), "port to listen for JSON-RPC connections")
                              ("httpport", boost::program_options::value<uint16_t>()->default_value(5680), "port to listen for HTTP JSON-RPC connections")
                              ("genesis-config", boost::program_options::value<std::string>()->default_value("genesis.dat"), 
                               "generate a genesis state with the given json file (only accepted when the blockchain is empty)")
                              ("version", "print the version information for bts_xt_client");


   boost::program_options::positional_options_description positional_config;
   positional_config.add("data-dir", 1);

   boost::program_options::variables_map option_variables;
   try
   {
     boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
       options(option_config).positional(positional_config).run(), option_variables);
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

   if (option_variables.count("version"))
   {
     std::cout << "bts_xt_client built on " << __DATE__ << " at " << __TIME__ << "\n";
     std::cout << "  bitshares_toolkit revision: " << bts::utilities::git_revision_sha << "\n";
     std::cout << "                              committed " << fc::get_approximate_relative_time_string(fc::time_point_sec(bts::utilities::git_revision_unix_timestamp)) << "\n";
     std::cout << "                 fc revision: " << fc::git_revision_sha << "\n";
     std::cout << "                              committed " << fc::get_approximate_relative_time_string(fc::time_point_sec(bts::utilities::git_revision_unix_timestamp)) << "\n";
     return 0;
   }

   try {
      print_banner();
      fc::path datadir = get_data_dir(option_variables);
      ::configure_logging(datadir);

      auto cfg   = load_config(datadir);
      auto chain = load_and_configure_chain_database(datadir, option_variables);
      auto wall  = std::make_shared<bts::wallet::wallet>(chain);
      wall->set_data_directory( datadir );

      auto c = std::make_shared<bts::client::client>( chain );
      _global_client = c.get();
      c->set_wallet( wall );
      c->run_delegate();

      bts::rpc::rpc_server_ptr rpc_server = std::make_shared<bts::rpc::rpc_server>();
      rpc_server->set_client(c);

      if( option_variables.count("server") )
      {
        // the user wants us to launch the RPC server.
        // First, override any config parameters they
        bts::rpc::rpc_server::config rpc_config(cfg.rpc);
        if (option_variables.count("rpcuser"))
          rpc_config.rpc_user = option_variables["rpcuser"].as<std::string>();
        if (option_variables.count("rpcpassword"))
          rpc_config.rpc_password = option_variables["rpcpassword"].as<std::string>();
        // for now, force binding to localhost only
        if (option_variables.count("rpcport"))
          rpc_config.rpc_endpoint = fc::ip::endpoint(fc::ip::address("127.0.0.1"), option_variables["rpcport"].as<uint16_t>());
        else
          rpc_config.rpc_endpoint = fc::ip::endpoint(fc::ip::address("127.0.0.1"), uint16_t(9988));
        if (option_variables.count("httpport"))
          rpc_config.httpd_endpoint = fc::ip::endpoint(fc::ip::address("127.0.0.1"), option_variables["httpport"].as<uint16_t>());
        std::cout<<"Starting json rpc server on "<< std::string( rpc_config.rpc_endpoint ) <<"\n";
        std::cout<<"Starting http json rpc server on "<< std::string( rpc_config.httpd_endpoint ) <<"\n";
        bool rpc_succuss = rpc_server->configure(rpc_config);
        if (!rpc_succuss)
        {
            std::cerr << "Error starting rpc server\n\n";
        }
      }
      else
      {
         std::cout << "Not starting rpc server, use --server to enable the rpc interface\n";
      }

      c->configure( datadir );
      if (option_variables.count("p2p-port"))
      {
         auto p2pport = option_variables["p2p-port"].as<uint16_t>();
         std::cout << "Listening to P2P connections on port "<<p2pport<<"\n";
         c->listen_on_port(p2pport);

         if( option_variables["upnp"].as<bool>() )
         {
            std::cout << "Attempting to map UPNP port...\n";
            auto upnp_service = new bts::net::upnp_service();
            upnp_service->map_port( p2pport );
            fc::usleep( fc::seconds(3) );
         }
      }
      c->connect_to_p2p_network();
      if (option_variables.count("connect-to"))
      {
         std::vector<std::string> hosts = option_variables["connect-to"].as<std::vector<std::string>>();
         for( auto peer : hosts )
         {
            c->connect_to_peer( peer );
         }
      }

      if( !option_variables.count("daemon") )
      {
         auto cli = std::make_shared<bts::cli::cli>( c, rpc_server );
         cli->wait();
      }
      else if( option_variables.count( "server" ) ) // daemon & server
      {
         rpc_server->wait_on_quit();
      }
      else // daemon  !server
      {
         std::cerr << "You must start the rpc server in daemon mode\n";
      }
   }
   catch ( const fc::exception& e )
   {
      std::cerr << "------------ error --------------\n" 
                << e.to_detail_string() << "\n";
      wlog( "${e}", ("e", e.to_detail_string() ) );
   }
   return 0;
}



void print_banner()
{
    std::cout<<"================================================================\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=             Welcome to BitShares "<< std::setw(5) << std::left << BTS_ADDRESS_PREFIX<<"                       =\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=  This software is in alpha testing and is not suitable for   =\n";
    std::cout<<"=  real monetary transactions or trading.  Use at your own     =\n";
    std::cout<<"=  risk.                                                       =\n";
    std::cout<<"=                                                              =\n";
    std::cout<<"=  Type 'help' for usage information.                          =\n";
    std::cout<<"================================================================\n";
}

void configure_logging(const fc::path& data_dir)
{
    fc::logging_config cfg;
    
    fc::file_appender::config ac;
    ac.filename = data_dir / "default.log";
    ac.truncate = false;
    ac.flush    = true;

    std::cout << "Logging to file \"" << ac.filename.generic_string() << "\"\n";
    
    fc::file_appender::config ac_rpc;
    ac_rpc.filename = data_dir / "rpc.log";
    ac_rpc.truncate = false;
    ac_rpc.flush    = true;

    std::cout << "Logging RPC to file \"" << ac_rpc.filename.generic_string() << "\"\n";
    
    cfg.appenders.push_back(fc::appender_config( "default", "file", fc::variant(ac)));
    cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac_rpc)));
    
    fc::logger_config dlc;
    dlc.level = fc::log_level::debug;
    dlc.name = "default";
    dlc.appenders.push_back("default");
    
    fc::logger_config dlc_rpc;
    dlc_rpc.level = fc::log_level::debug;
    dlc_rpc.name = "rpc";
    dlc_rpc.appenders.push_back("rpc");
    
    cfg.loggers.push_back(dlc);
    cfg.loggers.push_back(dlc_rpc);
    
    fc::configure_logging( cfg );
}


fc::path get_data_dir(const boost::program_options::variables_map& option_variables)
{ try {
   fc::path datadir;
   if (option_variables.count("data-dir"))
   {
     datadir = fc::path(option_variables["data-dir"].as<std::string>().c_str());
   }
   else
   {
#ifdef WIN32
     datadir =  fc::app_path() / "BitShares" BTS_ADDRESS_PREFIX;
#elif defined( __APPLE__ )
     datadir =  fc::app_path() / "BitShares" BTS_ADDRESS_PREFIX;
#else
     datadir = fc::app_path() / ".BitShares" BTS_ADDRESS_PREFIX;
#endif
   }
   return datadir;

} FC_RETHROW_EXCEPTIONS( warn, "error loading config" ) }

bts::blockchain::chain_database_ptr load_and_configure_chain_database(const fc::path& datadir,
                                                                      const boost::program_options::variables_map& option_variables)
{ try {
  std::cout << "Loading blockchain from \"" << ( datadir / "chain" ).generic_string()  << "\"\n";
  bts::blockchain::chain_database_ptr chain = std::make_shared<bts::blockchain::chain_database>();

  fc::path genesis_file = option_variables["genesis-config"].as<std::string>();
  std::cout << "Using genesis block from file \"" << fc::absolute( genesis_file ).string() << "\"\n";
  chain->open( datadir / "chain", genesis_file );

  return chain;
} FC_RETHROW_EXCEPTIONS( warn, "unable to open blockchain from ${data_dir}", ("data_dir",datadir/"chain") ) }

config load_config( const fc::path& datadir )
{ try {
      auto config_file = datadir/"config.json";
      config cfg;
      if( fc::exists( config_file ) )
      {
         std::cout << "Loading config \"" << config_file.generic_string()  << "\"\n";
         cfg = fc::json::from_file( config_file ).as<config>();
      }
      else
      {
         std::cerr<<"Creating default config file "<<config_file.generic_string()<<"\n";
         fc::json::save_to_file( cfg, config_file );
      }
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }
