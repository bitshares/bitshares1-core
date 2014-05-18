#include <boost/program_options.hpp>

#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/cli/cli.hpp>
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
                              ("port", boost::program_options::value<uint16_t>(), "set port to listen on")
                              ("connect-to", boost::program_options::value<std::string>(), "set remote host to connect to")
                              ("server", "enable JSON-RPC server")
                              ("rpcuser", boost::program_options::value<std::string>(), "username for JSON-RPC")
                              ("rpcpassword", boost::program_options::value<std::string>(), "password for JSON-RPC")
                              ("rpcport", boost::program_options::value<uint16_t>(), "port to listen for JSON-RPC connections")
                              ("httpport", boost::program_options::value<uint16_t>(), "port to listen for HTTP JSON-RPC connections")
                              ("genesis-json", boost::program_options::value<std::string>(), "generate a genesis block with the given json file (only for testing, only accepted when the blockchain is empty)");

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


   try {
      print_banner();
      fc::path datadir = get_data_dir(option_variables);
      ::configure_logging(datadir);

      auto cfg   = load_config(datadir);
      auto chain = load_and_configure_chain_database(datadir, option_variables);
      auto wall  = std::make_shared<bts::wallet::wallet>(chain);
      wall->set_data_directory( datadir );

      auto c = std::make_shared<bts::client::client>();
      _global_client = c.get();
      c->set_chain( chain );
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
      if (option_variables.count("port"))
        c->listen_on_port(option_variables["port"].as<uint16_t>());
      c->connect_to_p2p_network();
      if (option_variables.count("connect-to"))
        c->connect_to_peer(option_variables["connect-to"].as<std::string>());

      auto cli = std::make_shared<bts::cli::cli>( c, rpc_server );
      cli->wait();

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
    std::cout<<"=             Welcome to BitShares XT                          =\n";
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

    std::cout << "Logging to file " << ac.filename.generic_string() << "\n";
    
    fc::file_appender::config ac_rpc;
    ac_rpc.filename = data_dir / "rpc.log";
    ac_rpc.truncate = false;
    ac_rpc.flush    = true;

    std::cout << "Logging RPC to file " << ac_rpc.filename.generic_string() << "\n";
    
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
  std::cout << "Loading blockchain from " << ( datadir / "chain" ).generic_string()  << "\n";
  bts::blockchain::chain_database_ptr chain = std::make_shared<bts::blockchain::chain_database>();
  fc::optional<fc::path> genesis_file;
  if (option_variables.count("genesis-json"))
  {
    genesis_file = option_variables["genesis-json"].as<std::string>();
    std::cout << "Using genesis block from file \"" << genesis_file->string() << "\"\n";
  }
  chain->open( datadir / "chain", genesis_file );
  return chain;
} FC_RETHROW_EXCEPTIONS( warn, "unable to open blockchain from ${data_dir}", ("data_dir",datadir/"chain") ) }

config load_config( const fc::path& datadir )
{ try {
      auto config_file = datadir/"config.json";
      config cfg;
      if( fc::exists( config_file ) )
      {
         std::cout << "Loading config " << config_file.generic_string()  << "\n";
         cfg = fc::json::from_file( config_file ).as<config>();
      }
      else
      {
         std::cerr<<"Creating default config file "<<config_file.generic_string()<<"\n";
         fc::json::save_to_file( cfg, config_file );
      }
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }
