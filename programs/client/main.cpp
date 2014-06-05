#include <boost/program_options.hpp>

#include <bts/client/client.hpp>
#include <bts/net/upnp.hpp>
#include <bts/blockchain/chain_database.hpp>
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
#include <fc/io/json.hpp>

#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <fstream>

#include <iostream>
#include <iomanip>


struct config
{
   config() : default_peers{{"107.170.30.182:8764"}}, ignore_console(false) {}
   bts::rpc::rpc_server::config rpc;
   std::vector<std::string>     default_peers;
   bool                         ignore_console;
};

FC_REFLECT( config, (rpc)(default_peers)(ignore_console) )

using namespace boost;

void print_banner();
void configure_logging(const fc::path&);
fc::path get_data_dir(const program_options::variables_map& option_variables);
config   load_config( const fc::path& datadir );
void  load_and_configure_chain_database(const fc::path& datadir,
                                        const program_options::variables_map& option_variables);

bts::client::client* _global_client = nullptr;


int main( int argc, char** argv )
{
   // parse command-line options
   program_options::options_description option_config("Allowed options");
   option_config.add_options()("data-dir", program_options::value<std::string>(), "configuration data directory")
                              ("help", "display this help message")
                              ("p2p-port", program_options::value<uint16_t>(), "set port to listen on")
                              ("maximum-number-of-connections", program_options::value<uint16_t>(), "set the maximum number of peers this node will accept at any one time")
                              ("upnp", program_options::value<bool>()->default_value(true), "Enable UPNP")
                              ("connect-to", program_options::value<std::vector<std::string> >(), "set remote host to connect to")
                              ("server", "enable JSON-RPC server")
                              ("daemon", "run in daemon mode with no CLI console, starts JSON-RPC server")
                              ("rpcuser", program_options::value<std::string>(), "username for JSON-RPC") // default arguments are in config.json
                              ("rpcpassword", program_options::value<std::string>(), "password for JSON-RPC")
                              ("rpcport", program_options::value<uint16_t>(), "port to listen for JSON-RPC connections")
                              ("httpport", program_options::value<uint16_t>(), "port to listen for HTTP JSON-RPC connections")
                              ("genesis-config", program_options::value<std::string>()->default_value("genesis.json"), 
                               "generate a genesis state with the given json file (only accepted when the blockchain is empty)")
                              ("clear-peer-database", "erase all information in the peer database")
                              ("resync-blockchain", "delete our copy of the blockchain at startup, and download a fresh copy of the entire blockchain from the network")
                              ("version", "print the version information for bts_xt_client");


   program_options::positional_options_description positional_config;
   positional_config.add("data-dir", 1);

   program_options::variables_map option_variables;
   try
   {
     program_options::store(program_options::command_line_parser(argc, argv).
       options(option_config).positional(positional_config).run(), option_variables);
     program_options::notify(option_variables);
   }
   catch (program_options::error&)
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
      std::cout << fc::json::to_pretty_string( cfg ) <<"\n";

      load_and_configure_chain_database(datadir, option_variables);

      bts::client::client_ptr client = std::make_shared<bts::client::client>();
      client->open( datadir, option_variables["genesis-config"].as<std::string>() );
      _global_client = client.get();

      client->run_delegate();

      bts::rpc::rpc_server_ptr rpc_server = client->get_rpc_server();

      if( option_variables.count("server") || option_variables.count("daemon") )
      {
        // the user wants us to launch the RPC server.
        // First, override any config parameters they
       // bts::rpc::rpc_server::config rpc_config(cfg.rpc);
        if (option_variables.count("rpcuser"))
          cfg.rpc.rpc_user = option_variables["rpcuser"].as<std::string>();
        if (option_variables.count("rpcpassword"))
           cfg.rpc.rpc_password = option_variables["rpcpassword"].as<std::string>();
        if (option_variables.count("rpcport"))
           cfg.rpc.rpc_endpoint.set_port(option_variables["rpcport"].as<uint16_t>());
        if (option_variables.count("httpport"))
           cfg.rpc.httpd_endpoint.set_port(option_variables["httpport"].as<uint16_t>());

        if (cfg.rpc.rpc_user.empty() ||
            cfg.rpc.rpc_password.empty())
        {
          std::cout << "Error starting RPC server\n";
          std::cout << "You specified " << (option_variables.count("server") ? "--server" : "--daemon") << " on the command line,\n";
          std::cout << "but did not provide a username or password to authenticate RPC connections.\n";
          std::cout << "You can provide these by using --rpcuser=username and --rpcpassword=password on the\n";
          std::cout << "command line, or by setting the \"rpc_user\" and \"rpc_password\" properties in the\n";
          std::cout << "config file.\n";
          return 1;
        }

        // launch the RPC servers
        bool rpc_success = rpc_server->configure(cfg.rpc);

        // this shouldn't fail due to the above checks, but just to be safe...
        if (!rpc_success)
          std::cerr << "Error starting rpc server\n\n";

        fc::optional<fc::ip::endpoint> actual_rpc_endpoint = rpc_server->get_rpc_endpoint();
        if (actual_rpc_endpoint)
        {
          std::cout << "Starting JSON RPC server on port " << actual_rpc_endpoint->port();
          if (actual_rpc_endpoint->get_address() == fc::ip::address("127.0.0.1"))
            std::cout << " (localhost only)";
          std::cout << "\n";
        }

        fc::optional<fc::ip::endpoint> actual_httpd_endpoint = rpc_server->get_httpd_endpoint();
        if (actual_httpd_endpoint)
        {
          std::cout << "Starting HTTP JSON RPC server on port " << actual_httpd_endpoint->port();
          if (actual_httpd_endpoint->get_address() == fc::ip::address("127.0.0.1"))
            std::cout << " (localhost only)";
          std::cout << "\n";
        }
      }
      else
      {
         std::cout << "Not starting RPC server, use --server to enable the RPC interface\n";
      }

      client->configure( datadir );

      if (option_variables.count("maximum-number-of-connections"))
      {
        fc::mutable_variant_object params;
        params["maximum_number_of_connections"] = option_variables["maximum-number-of-connections"].as<uint16_t>();
        client->network_set_advanced_node_parameters(params);
      }

      if (option_variables.count("p2p-port"))
      {
         uint16_t p2pport = option_variables["p2p-port"].as<uint16_t>();
         client->listen_on_port(p2pport);

         if( option_variables["upnp"].as<bool>() )
         {
            std::cout << "Attempting to map P2P port " << p2pport << " with UPNP...\n";
            auto upnp_service = new bts::net::upnp_service();
            upnp_service->map_port( p2pport );
            fc::usleep( fc::seconds(3) );
         }
      }

      if (option_variables.count("clear-peer-database"))
      {
        std::cout << "Erasing old peer database\n";
        client->get_node()->clear_peer_database();
      }

      // fire up the p2p , 
      client->connect_to_p2p_network();
      fc::ip::endpoint actual_p2p_endpoint = client->get_p2p_listening_endpoint();
      std::cout << "Listening for P2P connections on ";
      if (actual_p2p_endpoint.get_address() == fc::ip::address())
        std::cout << "port " << actual_p2p_endpoint.port();
      else
        std::cout << (std::string)actual_p2p_endpoint;
      if (option_variables.count("p2p-port"))
      {
        uint16_t p2p_port = option_variables["p2p-port"].as<uint16_t>();
        if (p2p_port != 0 && p2p_port != actual_p2p_endpoint.port())
          std::cout << " (unable to bind to the desired port " << p2p_port << ")";
      }
      std::cout << "\n";


      if (option_variables.count("connect-to"))
      {
         std::vector<std::string> hosts = option_variables["connect-to"].as<std::vector<std::string>>();
         for( auto peer : hosts )
            client->connect_to_peer( peer );
      }
      else
      {
        for (std::string default_peer : cfg.default_peers)
          client->connect_to_peer(default_peer);
      }

      if( option_variables.count("daemon") || cfg.ignore_console )
      {
          std::cout << "Running in daemon mode, ignoring console\n";
          rpc_server->wait_on_quit();
      }
      else 
      {

#ifdef _DEBUG
        //tee cli output to the console and a log file
        fc::path console_log_file = datadir / "console.log";
        std::ofstream console_log(console_log_file.string());
        typedef boost::iostreams::tee_device<std::ostream, std::ofstream> TeeDevice;
        typedef boost::iostreams::stream<TeeDevice> TeeStream;
        TeeDevice my_tee(std::cout, console_log); 
        TeeStream cout_with_log(my_tee);
        //force flushing to console and log file whenever cin input is required
        std::cin.tie( &cout_with_log );
        auto cli = std::make_shared<bts::cli::cli>( client, std::cin, cout_with_log );
        //also echo input to the log file
        cli->set_input_log_stream(console_log);
#else
        auto cli = std::make_shared<bts::cli::cli>( client, std::cin, std::cout );
#endif
        cli->process_commands();
        cli->wait();
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


fc::path get_data_dir(const program_options::variables_map& option_variables)
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

void load_and_configure_chain_database( const fc::path& datadir,
                                        const program_options::variables_map& option_variables)
{ try {
  if (option_variables.count("resync-blockchain"))
  {
    std::cout << "Deleting old copy of the blockchain in \"" << ( datadir / "chain" ).generic_string() << "\"\n";
    try
    {
      fc::remove_all(datadir / "chain");
    }
    catch (const fc::exception& e)
    {
      std::cout << "Error while deleting old copy of the blockchain: " << e.what() << "\n";
      std::cout << "You may need to manually delete your blockchain and relaunch bitshares_client\n";
    }
  }
  else
  {
    std::cout << "Loading blockchain from \"" << ( datadir / "chain" ).generic_string()  << "\"\n";
  }

  fc::path genesis_file = option_variables["genesis-config"].as<std::string>();
  std::cout << "Using genesis block from file \"" << fc::absolute( genesis_file ).string() << "\"\n";

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
         std::cerr<<"Creating default config file \""<<config_file.generic_string()<<"\"\n";
         fc::json::save_to_file( cfg, config_file );
      }
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }
