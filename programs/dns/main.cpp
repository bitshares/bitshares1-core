#include <boost/program_options.hpp>
#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/dns/dns_cli.hpp>
#include <bts/dns/dns_rpc_server.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_wallet.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <iostream>

// refactor away:
#include <bts/net/chain_server.hpp>
/// 

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
std::shared_ptr<bts::dns::dns_db> load_and_configure_chain_database(const fc::path& datadir, 
								    const boost::program_options::variables_map& option_variables);

int main( int argc, char** argv )
{
   // parse command-line options
   boost::program_options::options_description option_config("Allowed options");
   option_config.add_options()("data-dir", boost::program_options::value<std::string>(), "configuration data directory")
                              ("help", "display this help message")
                              ("p2p", "enable p2p mode")
                              ("port", boost::program_options::value<uint16_t>(), "set port to listen on")
                              ("connect-to", boost::program_options::value<std::string>(), "set remote host to connect to")
                              ("server", "enable JSON-RPC server")
                              ("rpcuser", boost::program_options::value<std::string>(), "username for JSON-RPC")
                              ("rpcpassword", boost::program_options::value<std::string>(), "password for JSON-RPC")
                              ("rpcport", boost::program_options::value<uint16_t>(), "port to listen for JSON-RPC connections")
                              ("httpport", boost::program_options::value<uint16_t>(), "port to listen for HTTP JSON-RPC connections")
                              ("trustee-private-key", boost::program_options::value<std::string>(), "act as a trustee using the given private key")
                              ("trustee-address", boost::program_options::value<std::string>(), "trust the given BTS address to generate blocks")
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
 

   bool p2p_mode = option_variables.count("p2p") != 0;


   try {
      print_banner();
      fc::path datadir = get_data_dir(option_variables);
      ::configure_logging(datadir);

      auto cfg   = load_config(datadir);
//TODO change to dns
      auto chain = load_and_configure_chain_database(datadir, option_variables);
      auto wall    = std::make_shared<bts::dns::dns_wallet>();
      wall->set_data_directory( datadir );

      auto c = std::make_shared<bts::client::client>(p2p_mode);
      c->set_chain( chain );
      c->set_wallet( wall );


      if (option_variables.count("trustee-private-key"))
      {
         auto key = fc::variant(option_variables["trustee-private-key"].as<std::string>()).as<fc::ecc::private_key>();
         c->run_trustee(key);
      }
      else if( fc::exists( "trustee.key" ) )
      {
         auto key = fc::json::from_file( "trustee.key" ).as<fc::ecc::private_key>();
         c->run_trustee(key);
      }


      bts::dns::dns_rpc_server_ptr rpc_server = std::make_shared<bts::dns::dns_rpc_server>();
      rpc_server->set_client(c);
      auto cli = std::make_shared<bts::dns::dns_cli>( c, rpc_server );
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
        std::cerr<<"starting json rpc server on "<< std::string( rpc_config.rpc_endpoint ) <<"\n";
        std::cerr<<"starting http json rpc server on "<< std::string( rpc_config.httpd_endpoint ) <<"\n";
        rpc_server->configure(rpc_config);
      }
      
      if (p2p_mode)
      {
        c->configure( datadir );
        if (option_variables.count("port"))
          c->listen_on_port(option_variables["port"].as<uint16_t>());
        c->connect_to_p2p_network();
        if (option_variables.count("connect-to"))
          c->connect_to_peer(option_variables["connect-to"].as<std::string>());
      }
      else
	{
        c->add_node( "127.0.0.1:4569" );
}


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


void configure_logging(const fc::path& data_dir)
{
   fc::file_appender::config ac;
   ac.filename = data_dir / "log.txt";
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
     datadir =  fc::app_path() / "dotp2p";
#elif defined( __APPLE__ )
     datadir =  fc::app_path() / "dotp2p";
#else
     datadir = fc::app_path() / ".dotp2p";
#endif
   }
   return datadir;

} FC_RETHROW_EXCEPTIONS( warn, "error loading config" ) }


std::shared_ptr<bts::dns::dns_db> load_and_configure_chain_database(const fc::path& datadir, 
						          const boost::program_options::variables_map& option_variables)
{
  auto db = std::make_shared<bts::dns::dns_db>();
  db->open( datadir / "chain", true );
  if (option_variables.count("trustee-address"))
    db->set_trustee(bts::blockchain::address(option_variables["trustee-address"].as<std::string>()));
  else
    db->set_trustee(bts::blockchain::address("43cgLS17F2uWJKKFbPoJnnoMSacj"));
  if (option_variables.count("genesis-json"))
  {
    if (db->head_block_num() == uint32_t(-1))
    {
      fc::path genesis_json_file(option_variables["genesis-json"].as<std::string>());
      bts::blockchain::trx_block genesis_block;
      try
      {
        genesis_block = bts::net::create_test_genesis_block(genesis_json_file);
      }
      catch (fc::exception& e)
      {
        wlog("Error creating genesis block from file ${filename}: ${e}", ("filename", genesis_json_file)("e", e.to_string()));
        return db;
      }
      try 
      {
        db->push_block(genesis_block);
      }
      catch ( const fc::exception& e )
      {
        wlog( "error pushing genesis block to blockchain database: ${e}", ("e", e.to_detail_string() ) );
      }
    }
    else
      wlog("Ignoring genesis-json command-line argument because our blockchain already has a genesis block");
  }
  return db;
}




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
