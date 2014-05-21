#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/cli/cli.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>

#include <iostream>

#include <fc/network/http/server.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/ip.hpp>

#include "bts_xt_thread.h"

struct config
{
    config():ignore_console(false){}
    bts::rpc::rpc_server::config rpc;
    bool ignore_console;
};
FC_REFLECT( config, (rpc)(ignore_console) )


void try_open_wallet(bts::rpc::rpc_server_ptr);
void configure_logging(const fc::path&);
config   load_config( const fc::path& datadir );
bts::blockchain::chain_database_ptr load_and_configure_chain_database(const fc::path& datadir, 
                                                                      const boost::program_options::variables_map& option_variables);

void BtsXtThread::run() {

    try {
        
        ::configure_logging(_datadir);
                
        auto cfg   = load_config(_datadir);
        auto chain = load_and_configure_chain_database(_datadir, _option_variables);
        auto wall  = std::make_shared<bts::wallet::wallet>(chain);
        wall->set_data_directory( _datadir );
        
        auto c = std::make_shared<bts::client::client>();
        c->set_chain( chain );
        c->set_wallet( wall );
        c->run_delegate();
        
        bts::rpc::rpc_server_ptr rpc_server = std::make_shared<bts::rpc::rpc_server>();
        rpc_server->set_client(c);
        
        std::cout << "starting rpc server..\n";
        fc_ilog( fc::logger::get("rpc"), "starting rpc server..");
        bts::rpc::rpc_server::config rpc_config(cfg.rpc);
        if (_option_variables.count("rpcuser"))
            rpc_config.rpc_user = _option_variables["rpcuser"].as<std::string>();
        if (_option_variables.count("rpcpassword"))
            rpc_config.rpc_password = _option_variables["rpcpassword"].as<std::string>();
        // for now, force binding to localhost only
        if (_option_variables.count("rpcport"))
            rpc_config.rpc_endpoint = fc::ip::endpoint(fc::ip::address("127.0.0.1"), _option_variables["rpcport"].as<uint16_t>());
        if (_option_variables.count("httpport"))
            rpc_config.httpd_endpoint = fc::ip::endpoint(fc::ip::address("127.0.0.1"), _option_variables["httpport"].as<uint16_t>());
        std::cout<<"Starting json rpc server on "<< std::string( rpc_config.rpc_endpoint ) <<"\n";
        std::cout<<"Starting http json rpc server on "<< std::string( rpc_config.httpd_endpoint ) <<"\n";     
        try_open_wallet(rpc_server); // assuming password is blank
        rpc_server->configure(rpc_config);
                
       
      c->configure( _datadir );
      if (_option_variables.count("port"))
        c->listen_on_port(_option_variables["port"].as<uint16_t>());
      c->connect_to_p2p_network();
      if (_option_variables.count("connect-to"))
        c->connect_to_peer(_option_variables["connect-to"].as<std::string>());        

        while(!_cancel) fc::usleep(fc::microseconds(10000));
        
        wall->close();
    } 
   catch ( const fc::exception& e )
   {
      std::cerr << "------------ error --------------\n" 
                << e.to_string() << "\n";
      wlog( "${e}", ("e", e.to_detail_string() ) );
   }
}

void try_open_wallet(bts::rpc::rpc_server_ptr rpc_server) {
    try
    {
        // try to open without a passphrase first
        rpc_server->direct_invoke_method("open_wallet", fc::variants());
        return;
    }
    catch (bts::rpc::rpc_wallet_passphrase_incorrect_exception&)
    {
    }
    catch (const fc::exception& e)
    {
    }
    catch (...)
    {
    }
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

bts::blockchain::chain_database_ptr load_and_configure_chain_database(const fc::path& datadir,
                                                                      const boost::program_options::variables_map& option_variables)
{ try {
  std::cout << "Loading blockchain from " << ( datadir / "chain" ).generic_string()  << "\n";
  bts::blockchain::chain_database_ptr chain = std::make_shared<bts::blockchain::chain_database>();
  chain->open( datadir / "chain" );
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
