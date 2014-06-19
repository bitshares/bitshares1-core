#define DEFAULT_LOGGER "client" 

#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/cli/cli.hpp>
#include <bts/net/node.hpp>
#include <bts/net/upnp.hpp>
#include <bts/net/peer_database.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/git_revision.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/api/common_api.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/config.hpp>

#include <bts/db/level_map.hpp>

#include <fc/reflect/variant.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>

#include <fc/thread/thread.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/raw.hpp>

#include <fc/filesystem.hpp>
#include <fc/git_revision.hpp>

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm/reverse.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/program_options.hpp>
#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread/mutex.hpp>

#include <iostream>
#include <algorithm>
#include <fstream>
#include <iomanip>

#include <boost/lexical_cast.hpp>
using namespace boost;
   using std::string;


namespace bts { namespace client {

const string BTS_MESSAGE_MAGIC = "BitShares Signed Message:\n";

void print_banner();
fc::logging_config create_default_logging_config(const fc::path&);
fc::path get_data_dir(const program_options::variables_map& option_variables);
config   load_config( const fc::path& datadir );
void  load_and_configure_chain_database(const fc::path& datadir,
                                        const program_options::variables_map& option_variables);
fc::variant_object version_info();

program_options::variables_map parse_option_variables(int argc, char** argv)
{
   // parse command-line options
   program_options::options_description option_config("Allowed options");
   option_config.add_options()("data-dir", program_options::value<string>(), "configuration data directory")
                              ("input-log", program_options::value<std::string>(), 
                                 "log file with CLI commands to execute at startup")
                              ("help", "display this help message and exit")
                              ("p2p-port", program_options::value<uint16_t>(), "set port to listen on")
                              ("maximum-number-of-connections", program_options::value<uint16_t>(), 
                                  "set the maximum number of peers this node will accept at any one time")
                              ("upnp", program_options::value<bool>()->default_value(true), "Enable UPNP")
                              ("connect-to", program_options::value<std::vector<string> >(), "set remote host to connect to")
                              ("disable-default-peers", "disable automatic connection to default peers")
                              ("server", "enable JSON-RPC server")
                              ("daemon", "run in daemon mode with no CLI console, starts JSON-RPC server")
                              ("rpcuser", program_options::value<string>(), "username for JSON-RPC")
                              ("rpcpassword", program_options::value<string>(), "password for JSON-RPC")
                              ("rpcport", program_options::value<uint16_t>(), "port to listen for JSON-RPC connections")
                              ("httpport", program_options::value<uint16_t>(), "port to listen for HTTP JSON-RPC connections")
                              ("genesis-config", program_options::value<string>(), 
                                 "generate a genesis state with the given json file instead of using the built-in "
                                 "genesis block (only accepted when the blockchain is empty)")
                              ("clear-peer-database", "erase all information in the peer database")
                              ("resync-blockchain", "delete our copy of the blockchain at startup, and download a "
                                 "fresh copy of the entire blockchain from the network")
                              ("version", "print version information and exit")
                              ("total-bandwidth-limit", program_options::value<uint32_t>()->default_value(100000), 
                                  "Limit total bandwidth to this many bytes per second");


  program_options::variables_map option_variables;
  try
  {
    program_options::store(program_options::command_line_parser(argc, argv).
      options(option_config).run(), option_variables);
    program_options::notify(option_variables);
  }
  catch (program_options::error&)
  {
    std::cerr << "Error parsing command-line options\n\n";
    std::cerr << option_config << "\n";
    exit(1);
  }

  if (option_variables.count("help"))
  {
    std::cout << option_config << "\n";
    exit(0);
  }
  else if (option_variables.count("version"))
  {
    std::cout << fc::json::to_pretty_string( bts::client::version_info() ) << "\n";
    exit(0);
  }

  return option_variables;
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

fc::logging_config create_default_logging_config(const fc::path& data_dir)
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

    fc::file_appender::config ac_p2p;
    ac_p2p.filename = data_dir / "p2p.log";
    ac_p2p.truncate = false;
#ifdef NDEBUG
    ac_p2p.flush    = false;
#else // NDEBUG
    ac_p2p.flush    = true;
#endif // NDEBUG

    std::cout << "Logging P2P to file \"" << ac_p2p.filename.generic_string() << "\"\n";

    fc::variants  c  {
                fc::mutable_variant_object( "level","debug")("color", "green"),
                fc::mutable_variant_object( "level","warn")("color", "brown"),
                fc::mutable_variant_object( "level","error")("color", "red") };

     cfg.appenders.push_back( 
            fc::appender_config( "stderr", "console", 
                fc::mutable_variant_object()
                    ( "stream","std_error")
                    ( "level_colors", c ) 
                ) ); 

    cfg.appenders.push_back(fc::appender_config( "default", "file", fc::variant(ac)));
    cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac_rpc)));
    cfg.appenders.push_back(fc::appender_config( "p2p", "file", fc::variant(ac_p2p)));
    
    fc::logger_config dlc;
    dlc.level = fc::log_level::debug;
    dlc.name = "default"; 
    dlc.appenders.push_back("default");
    dlc.appenders.push_back("p2p");
   // dlc.appenders.push_back("stderr");
    
    fc::logger_config dlc_rpc;
    dlc_rpc.level = fc::log_level::debug;
    dlc_rpc.name = "rpc";
    dlc_rpc.appenders.push_back("rpc");
    
    fc::logger_config dlc_p2p;
    dlc_p2p.level = fc::log_level::debug;
    dlc_p2p.name = "p2p";
    dlc_p2p.appenders.push_back("p2p");
    
    cfg.loggers.push_back(dlc);
    cfg.loggers.push_back(dlc_rpc);
    cfg.loggers.push_back(dlc_p2p);
    
    return cfg;
}


fc::path get_data_dir(const program_options::variables_map& option_variables)
{ try {
   fc::path datadir;
   if (option_variables.count("data-dir"))
   {
     datadir = fc::path(option_variables["data-dir"].as<string>().c_str());
   }
   else
   {
#ifdef WIN32
     datadir =  fc::app_path() / BTS_BLOCKCHAIN_NAME;
#elif defined( __APPLE__ )
     datadir =  fc::app_path() / BTS_BLOCKCHAIN_NAME;
#else
     std::string blockchain_name(BTS_BLOCKCHAIN_NAME);
     std::string::iterator end_pos = std::remove(blockchain_name.begin(), blockchain_name.end(), ' ');
     blockchain_name.erase(end_pos, blockchain_name.end());
     datadir = fc::app_path() / ("." + blockchain_name);
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
      std::cout << "You may need to manually delete your blockchain and relaunch the client\n";
    }
  }
  else
  {
    std::cout << "Loading blockchain from \"" << ( datadir / "chain" ).generic_string()  << "\"\n";
  }

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
         cfg.logging = create_default_logging_config(datadir);
         fc::json::save_to_file( cfg, config_file );
      }
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }




   using namespace bts::wallet;
   using namespace bts::blockchain;

  typedef boost::iostreams::tee_device<std::ostream, std::ofstream> TeeDevice;
  typedef boost::iostreams::stream<TeeDevice> TeeStream;

    namespace detail
    {
       class client_impl : public bts::net::node_delegate,
                           public bts::api::common_api
       {
          public:
            class user_appender : public fc::appender
            {
               public:
                  user_appender( client_impl& c )
                  :_client_impl(c){}

                  virtual void log( const fc::log_message& m ) override
                  {
                     auto format = m.get_format();
                     // lookup translation on format here

                     // perform variable substitution;
                     string message = format_string( format, m.get_data() );
                     

                     { // appenders can be called from any thread
                        fc::scoped_lock<boost::mutex> lock(_history_lock);
                        _history.emplace_back( message );
                        if( _client_impl._cli )
                           _client_impl._cli->display_status_message( message );
                     }

                     // call a callback to the client...

                     // we need an RPC call to fetch this log and display the
                     // current status.
                  }

                  vector<string> get_history()const
                  {
                     fc::scoped_lock<boost::mutex> lock(_history_lock);
                     return _history;
                  }

                  void clear_history()
                  {
                     fc::scoped_lock<boost::mutex> lock(_history_lock);
                     _history.clear();
                  }

               private:
                  mutable boost::mutex  _history_lock;
                  // TODO: consider a deque and enforce maximum length?
                  vector<string>        _history;
                  client_impl&          _client_impl;
            };

            fc::shared_ptr<user_appender> _user_appender;

            client_impl(bts::client::client* self) :
              _self(self),_cli()
            { try {
                _user_appender = fc::shared_ptr<user_appender>( new user_appender(*this) );
                fc::logger::get( "user" ).add_appender( _user_appender );

                try {
                  _rpc_server = std::make_shared<rpc_server>(self);
                } FC_RETHROW_EXCEPTIONS(warn,"rpc server")
                try {
                  _chain_db = std::make_shared<chain_database>();
                } FC_RETHROW_EXCEPTIONS(warn,"chain_db")
            } FC_RETHROW_EXCEPTIONS( warn, "" ) }

            virtual ~client_impl()override { delete _cli; }

            void start()
            {
                _cli->start();
            }



            void delegate_loop();
            void configure_rpc_server(config& cfg, const program_options::variables_map& option_variables);

            block_fork_data on_new_block(const full_block& block, const block_id_type& block_id, bool sync_mode);
            void on_new_transaction(const signed_transaction& trx);

            /* Implement node_delegate */
            // @{
            virtual bool has_item(const bts::net::item_id& id) override;
            virtual bool handle_message(const bts::net::message&, bool sync_mode) override;
            virtual std::vector<bts::net::item_hash_t> get_item_ids(uint32_t item_type,
                                                                    const std::vector<bts::net::item_hash_t>& blockchain_synopsis,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit = 2000) override;
            virtual bts::net::message get_item(const bts::net::item_id& id) override;
            virtual fc::sha256 get_chain_id() const override
            { 
                FC_ASSERT( _chain_db != nullptr );
                return _chain_db->chain_id(); 
            }
            virtual std::vector<bts::net::item_hash_t> get_blockchain_synopsis(uint32_t item_type, 
                                                                               bts::net::item_hash_t reference_point = bts::net::item_hash_t(),
                                                                               uint32_t number_of_blocks_after_reference_point = 0) override;
            virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
            virtual void connection_count_changed(uint32_t c) override;
            virtual uint32_t get_block_number(bts::net::item_hash_t block_id) override;
            virtual void error_encountered(const std::string& message, const fc::oexception& error) override;
            /// @}
            bts::client::client*                                        _self;
            bts::cli::cli*                                              _cli;

            std::ofstream                                               _console_log;
            std::unique_ptr<std::ostream>                               _output_stream;
            std::unique_ptr<TeeDevice>                                  _tee_device;
            std::unique_ptr<TeeStream>                                  _tee_stream;
            std::unique_ptr<std::istream>                               _command_script_holder;

            fc::time_point                                              _last_block;
            fc::path                                                    _data_dir;

            bts::rpc::rpc_server_ptr                                    _rpc_server;
            bts::net::node_ptr                                          _p2p_node;
            std::unique_ptr<bts::net::upnp_service>                     _upnp_service;
            chain_database_ptr                                          _chain_db;
            unordered_map<transaction_id_type, signed_transaction>      _pending_trxs;
            wallet_ptr                                                  _wallet;
            fc::future<void>                                            _delegate_loop_complete;
            fc::time_point                                              _last_sync_status_message_time;

            bts::db::level_map<fc::time_point,fc::exception>            _exception_db;

            //-------------------------------------------------- JSON-RPC Method Implementations
            // include all of the method overrides generated by the bts_api_generator
            // this file just contains a bunch of lines that look like:
            // virtual void some_method(const string& some_arg) override;
#include <bts/rpc_stubs/common_api_overrides.ipp> //include auto-generated RPC API declarations
       };

      //should this function be moved to rpc server eventually? probably...
      void client_impl::configure_rpc_server(config& cfg, 
                                             const program_options::variables_map& option_variables)
      {
        if( option_variables.count("server") || option_variables.count("daemon") )
        {
          // the user wants us to launch the RPC server.
          // First, override any config parameters they
          // bts::rpc::rpc_server::config rpc_config(cfg.rpc);
          if (option_variables.count("rpcuser"))
            cfg.rpc.rpc_user = option_variables["rpcuser"].as<string>();
          if (option_variables.count("rpcpassword"))
              cfg.rpc.rpc_password = option_variables["rpcpassword"].as<string>();
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
            exit(1);
          }

          // launch the RPC servers
          bool rpc_success = _rpc_server->configure(cfg.rpc);

          // this shouldn't fail due to the above checks, but just to be safe...
          if (!rpc_success)
            std::cerr << "Error starting rpc server\n\n";

          fc::optional<fc::ip::endpoint> actual_rpc_endpoint = _rpc_server->get_rpc_endpoint();
          if (actual_rpc_endpoint)
          {
            std::cout << "Starting JSON RPC server on port " << actual_rpc_endpoint->port();
            if (actual_rpc_endpoint->get_address() == fc::ip::address("127.0.0.1"))
              std::cout << " (localhost only)";
            std::cout << "\n";
          }

          fc::optional<fc::ip::endpoint> actual_httpd_endpoint = _rpc_server->get_httpd_endpoint();
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

      }

       void client_impl::delegate_loop()
       {
          fc::usleep( fc::seconds( 1 ) );
         _last_block = _chain_db->get_head_block().timestamp;
         while( !_delegate_loop_complete.canceled() )
         {
            auto now = bts::blockchain::now(); //fc::time_point_sec(fc::time_point::now());
            auto next_block_time = _wallet->next_block_production_time();
           // ilog( "next block time: ${b}  interval: ${i} seconds  now: ${n}",
           //       ("b",next_block_time)("i",BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)("n",now) );
            if( next_block_time < (now + -1) ||
                (next_block_time - now) > fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) )
            {
               fc::usleep( fc::seconds(2) );
               continue;
            }
            else
            { 
               try {
                  FC_ASSERT( _wallet->is_unlocked(), "Wallet must be unlocked to produce blocks" );
                  FC_ASSERT( network_get_connection_count() >= BTS_MIN_DELEGATE_CONNECTION_COUNT,
                             "Client must have ${count} connections before you may produce blocks",
                             ("count",BTS_MIN_DELEGATE_CONNECTION_COUNT) );
                  ilog( "producing block in: ${b}", ("b",(next_block_time-now).count()/1000000.0) );

                  fc::usleep( (next_block_time - now) );
                  full_block next_block = _chain_db->generate_block( next_block_time );
                  _wallet->sign_block( next_block );

                  on_new_block(next_block, next_block.id(), false);
                  _p2p_node->broadcast(block_message( next_block ));
               } 
               catch ( const fc::exception& e )
               {
                  _exception_db.store( fc::time_point::now(), e);
               }
            }
            fc::usleep( fc::seconds(1) );
         }
       } // delegate_loop

       vector<account_id_type> client_impl::blockchain_list_current_round_active_delegates()
       {
          return _chain_db->current_round_active_delegates();
       }

       vector<block_record> client_impl::blockchain_list_blocks( int32_t first, uint32_t count)
       {
          FC_ASSERT( first >= 0 );
          FC_ASSERT( count <= 1000 );
          vector<block_record> result;

          int32_t last = std::min<int32_t>( first+count-1, _chain_db->get_head_block_num() );
          if ( last < first )
          {
             return result;
          }
          
          result.reserve( last - first + 1);

          for( int32_t block_num = first; block_num <= last; ++block_num )
             result.push_back( *_chain_db->get_block_record( block_num ) );

          return result;
       }

       signed_transactions client_impl::blockchain_get_pending_transactions() const
       {
         signed_transactions trxs;
         auto pending = _chain_db->get_pending_transactions();
         trxs.reserve(pending.size());
         for (auto trx_eval_ptr : pending)
         {
           trxs.push_back(trx_eval_ptr->trx);
         }
         return trxs;
       }

       ///////////////////////////////////////////////////////
       // Implement chain_client_delegate                   //
       ///////////////////////////////////////////////////////
       block_fork_data client_impl::on_new_block(const full_block& block, const block_id_type& block_id, bool sync_mode)
       {
         try 
         {
            try
            {
              ilog("Received a new block from the p2p network, current head block is ${num}, "
                   "new block is ${block}, current head block is ${num}",
                   ("num", _chain_db->get_head_block_num())("block", block)("num", _chain_db->get_head_block_num()));
              fc::optional<block_fork_data> fork_data = _chain_db->get_block_fork_data( block_id );
            
              if( fork_data && fork_data->is_known )
              {
                if (sync_mode && !fork_data->is_linked)
                   FC_THROW_EXCEPTION(bts::blockchain::unlinkable_block, 
                                      "The blockchain already has this block, but it isn't linked");
                ilog("The block we just received is one I've already seen, ignoring it");
                return *fork_data;
              }
              else
              {
                   block_fork_data result = _chain_db->push_block(block);
                   if (sync_mode && !result.is_linked)
                      FC_THROW_EXCEPTION(bts::blockchain::unlinkable_block, "The blockchain accepted this block, but it isn't linked");
                   ilog("After push_block, current head block is ${num}", ("num", _chain_db->get_head_block_num()));
            
                   if (_cli && 
                       result.is_included && 
                       (fc::time_point::now() - block.timestamp) > fc::minutes(5) &&
                       _last_sync_status_message_time < (fc::time_point::now() - fc::seconds(30)))
                   {
                      std::ostringstream message;
                      message << "--- syncing with p2p network, our last block was created " 
                              << fc::get_approximate_relative_time_string(block.timestamp);
                      ulog( message.str() );
                      _last_sync_status_message_time = fc::time_point::now();
                   }
            
                   return result;
              }
            } FC_RETHROW_EXCEPTIONS(warn, "Error pushing block ${block_number} - ${block_id}", 
                                    ("block_id",block.id())
                                    ("block_number",block.block_num)
                                    ("block", block) );
         } 
         catch ( const fc::exception& e )
         {
            _exception_db.store( fc::time_point::now(), e);
            throw;
         }
       }

       void client_impl::on_new_transaction(const signed_transaction& trx)
       { 
          try {
              // throws exception if invalid trx.
              _chain_db->store_pending_transaction(trx); 
          } 
          catch ( const fc::exception& e )
          {
             _exception_db.store( fc::time_point::now(), e );
             throw;
          }
       }

       ///////////////////////////////////////////////////////
       // Implement node_delegate                           //
       ///////////////////////////////////////////////////////
       bool client_impl::has_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
           return _chain_db->is_known_block( id.item_hash );
         }

         if (id.item_type == trx_message_type)
         {
           return _chain_db->is_known_transaction( id.item_hash );
         }
         return false;
       }

       bool client_impl::handle_message(const bts::net::message& message_to_handle, bool sync_mode)
       {
         switch (message_to_handle.msg_type)
         {
            case block_message_type:
              {
                block_message block_message_to_handle(message_to_handle.as<block_message>());
                ilog("CLIENT: just received block ${id}", ("id", block_message_to_handle.block.id()));
                bts::blockchain::block_id_type old_head_block = _chain_db->get_head_block_id();
                block_fork_data fork_data = on_new_block(block_message_to_handle.block, block_message_to_handle.block_id, sync_mode);
                return fork_data.is_included ^ (block_message_to_handle.block.previous == old_head_block);  // TODO is this right?
              }
            case trx_message_type:
              {
                trx_message trx_message_to_handle(message_to_handle.as<trx_message>());
                ilog("CLIENT: just received transaction ${id}", ("id", trx_message_to_handle.trx.id()));
                on_new_transaction(trx_message_to_handle.trx);
                return false;
              }
         }
         return false;
       }

      /**
      *  Get the hash of all blocks after from_id
      */
      std::vector<bts::net::item_hash_t> client_impl::get_item_ids(uint32_t item_type,
                                                                  const std::vector<bts::net::item_hash_t>& blockchain_synopsis,
                                                                  uint32_t& remaining_item_count,
                                                                  uint32_t limit /* = 2000 */)
      {
        // limit = 20; // for testing
        FC_ASSERT(item_type == bts::client::block_message_type);
        uint32_t last_seen_block_num = 1;
        bts::net::item_hash_t last_seen_block_hash;
        for (const bts::net::item_hash_t& item_hash : boost::adaptors::reverse(blockchain_synopsis))
        {
          try
          {
            uint32_t block_num = _chain_db->get_block_num(item_hash);
            if (_chain_db->is_included_block(item_hash))
            {
              last_seen_block_num = block_num;
              last_seen_block_hash = item_hash;
              break;
            }
          }
          catch (fc::key_not_found_exception&)
          {
          }
        }

        std::vector<bts::net::item_hash_t> hashes_to_return;
        uint32_t head_block_num = _chain_db->get_head_block_num();
        if (head_block_num == 0)
        {
          remaining_item_count = 0;
          return hashes_to_return; // we have no blocks
        }

        if (last_seen_block_num > head_block_num)
        {
          // We were getting this condition during testing when one of the blocks is invalid because 
          // its timestamp was in the future.  It was accepted in to the database, but never linked to
          // the chain.  We've fixed the test and it doesn't seem likely that this would happen in a
          // production environment.

          //wlog("last_seen_block_num(${last_seen}) > head_block_num(${head})", ("last_seen", last_seen_block_num)("head", head_block_num));
          //wlog("last_seen_block(${last_seen}) > head_block(${head})", ("last_seen", last_seen_block_hash)("head", _chain_db->get_head_block_id()));
          //int num = rand() % 100;
          //fc::path dot_filename(std::string("E:\\fork") + boost::lexical_cast<std::string>(num) + ".dot");
          //_chain_db->export_fork_graph(dot_filename);
          //wlog("Graph written to file ${dot_filename}", ("dot_filename", dot_filename));

          assert(false);
          // and work around it
          last_seen_block_num = head_block_num;
        }

        remaining_item_count = head_block_num - last_seen_block_num + 1;
        uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
        hashes_to_return.reserve(items_to_get_this_iteration);
        for (uint32_t i = 0; i < items_to_get_this_iteration; ++i)
        {
          signed_block_header header;
          try
          {
            header = _chain_db->get_block(last_seen_block_num);
          }
          catch (fc::key_not_found_exception&)
          {
            ilog("chain_database::get_block failed to return block number ${last_seen_block_num} even though chain_database::get_block_num() provided its block number", 
                 ("last_seen_block_num",last_seen_block_num));
            assert( !"I assume this can never happen");
          }
          hashes_to_return.push_back(header.id());
          ++last_seen_block_num;
        }
        remaining_item_count -= items_to_get_this_iteration;
        return hashes_to_return;
      }

      std::vector<bts::net::item_hash_t> client_impl::get_blockchain_synopsis(uint32_t item_type, 
                                                                              bts::net::item_hash_t reference_point /* = bts::net::item_hash_t() */, 
                                                                              uint32_t number_of_blocks_after_reference_point /* = 0 */)
      {
        FC_ASSERT(item_type == bts::client::block_message_type);
        std::vector<bts::net::item_hash_t> synopsis;
        uint32_t high_block_num = 0;
        uint32_t non_fork_high_block_num = 0;
        std::vector<block_id_type> fork_history;

        if (reference_point != bts::net::item_hash_t())
        {
          // the node is asking for a summary of the block chain up to a specified
          // block, which may or may not be on a fork
          // for now, assume it's not on a fork
          try
          {
            if (_chain_db->is_included_block(reference_point))
            {
              // block is a block we know about and is on the main chain
              uint32_t reference_point_block_num = _chain_db->get_block_num(reference_point);
              assert(reference_point_block_num > 0);
              high_block_num = reference_point_block_num;
              non_fork_high_block_num = high_block_num;
            }
            else
            {
              // block is a block we know about, but it is on a fork
              try
              {
                fork_history = _chain_db->get_fork_history(reference_point);
                assert(fork_history.size() >= 2);
                assert(fork_history.front() == reference_point);
                block_id_type last_non_fork_block = fork_history.back();
                fork_history.pop_back();
                boost::reverse(fork_history);
                try
                {
                  non_fork_high_block_num = _chain_db->get_block_num(last_non_fork_block);
                  assert(non_fork_high_block_num > 0);
                }
                catch (const fc::key_not_found_exception&)
                {
                  assert(!"get_fork_history() returned a history that doesn't link to the main chain");
                }
                high_block_num = non_fork_high_block_num + fork_history.size();
                assert(high_block_num == _chain_db->get_block_header(fork_history.back()).block_num);
              }
              catch (const fc::exception& e)
              {
                // unable to get fork history for some reason.  maybe not linked?
                // we can't return a synopsis of its chain
                elog("Unable to construct a blockchain synopsis for reference hash ${hash}: ${exception}", ("hash", reference_point)("exception", e));
                return synopsis;
              }
            }
          }
          catch (const fc::key_not_found_exception&)
          {
            assert(false); // the logic in the p2p networking code shouldn't call this with a reference_point that we've never seen
            // we've never seen this block
            return synopsis;
          }
        }
        else
        {
          // no reference point specified, summarize the whole block chain
          high_block_num = _chain_db->get_head_block_num();
          non_fork_high_block_num = high_block_num;
          if (high_block_num == 0)
            return synopsis; // we have no blocks
        }

        uint32_t true_high_block_num = high_block_num + number_of_blocks_after_reference_point;
        uint32_t low_block_num = 1;
        do
        {
          if (low_block_num <= non_fork_high_block_num)
            synopsis.push_back(_chain_db->get_block(low_block_num).id());
          else
            synopsis.push_back(fork_history[low_block_num - non_fork_high_block_num - 1]);
          low_block_num += ((true_high_block_num - low_block_num + 2) / 2);
        }
        while (low_block_num <= high_block_num);

        return synopsis;
      }

       bts::net::message client_impl::get_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
        //   uint32_t block_number = _chain_db->get_block_num(id.item_hash);
           bts::client::block_message block_message_to_send(_chain_db->get_block(id.item_hash));
           FC_ASSERT(id.item_hash == block_message_to_send.block_id); //.id());
        //   block_message_to_send.signature = block_message_to_send.block.delegate_signature;
           return block_message_to_send;
         }

         if (id.item_type == trx_message_type)
         {
           trx_message trx_message_to_send;
           auto iter = _pending_trxs.find(id.item_hash);
           if (iter != _pending_trxs.end())
             trx_message_to_send.trx = iter->second;
         }

         FC_THROW_EXCEPTION(fc::key_not_found_exception, "I don't have the item you're looking for");
       }

       string client_impl::execute_command_line(const string& input) const
       {
           if (_cli)
           {
              std::stringstream output;
               _cli->execute_command_line( input, &output );
               return output.str();
           }
           else
           {
              return "CLI not set for this client.\n";
           }
       }
       
       fc::variants client_impl::batch(const std::string& method_name, const std::vector<fc::variants>& parameters_list) const
       {
          fc::variants result;
          for ( auto parameters : parameters_list )
          {
             result.push_back( _self->get_rpc_server()->direct_invoke_method( method_name, parameters) );
          }
          return result;
       }

       void client_impl::sync_status(uint32_t item_type, uint32_t item_count)
       {
         if (_cli && _last_sync_status_message_time < (fc::time_point::now() - fc::seconds(10)))
         {
           std::ostringstream message;
           if (item_count > 100)
              message << "--- syncing with p2p network, " << item_count << " blocks left to fetch";
           else if (item_count == 0)
              message << "--- in sync with p2p network";
           if (!message.str().empty())
               ulog( message.str() );
           _last_sync_status_message_time = fc::time_point::now();
         }
       }

       void client_impl::connection_count_changed(uint32_t c)
       {
         std::ostringstream message;
         message << "--- there are now " << c << " active connections to the p2p network";
         ulog( message.str() );
       }

       uint32_t client_impl::get_block_number(bts::net::item_hash_t block_id)
       {
         return _chain_db->get_block_num(block_id);
       }

      void client_impl::error_encountered(const std::string& message, const fc::oexception& error)
      {
        if (error)
          _exception_db.store(fc::time_point::now(), *error);
        else
          _exception_db.store(fc::time_point::now(), fc::exception(FC_LOG_MESSAGE(error, message.c_str())));
        ulog( message );
      }

    } // end namespace detail

    client::client()
    :my( new detail::client_impl(this))
    {
    }

    client::client(bts::net::simulated_network_ptr network_to_connect_to)
    : my( new detail::client_impl(this) )
    {
      network_to_connect_to->add_node_delegate(my.get());
      my->_p2p_node = network_to_connect_to;
    }

    void client::open( const path& data_dir, fc::optional<fc::path> genesis_file_path )
    { try {
        my->_exception_db.open( data_dir / "exceptions", true );
        my->_chain_db->open( data_dir / "chain", genesis_file_path );
        my->_wallet = std::make_shared<bts::wallet::wallet>( my->_chain_db );
        my->_wallet->set_data_directory( data_dir / "wallets" );

        //if we are using a simulated network, _p2p_node will already be set by client's constructor
        if (!my->_p2p_node)
        {
          my->_p2p_node = std::make_shared<bts::net::node>();
        }
        my->_p2p_node->set_node_delegate(my.get());


    } FC_RETHROW_EXCEPTIONS( warn, "", ("data_dir",data_dir) ) }


    client::~client()
    {
       try {
          if( my->_delegate_loop_complete.valid() )
          {
             my->_delegate_loop_complete.cancel();
             ilog( "waiting for delegate loop to complete" );
             my->_delegate_loop_complete.wait();
          }
       }
       catch ( const fc::canceled_exception& ) {}
       catch ( const fc::exception& e )
       {
          wlog( "${e}", ("e",e.to_detail_string() ) );
       }
    }

    wallet_ptr client::get_wallet()const { return my->_wallet; }
    chain_database_ptr client::get_chain()const { return my->_chain_db; }
    bts::rpc::rpc_server_ptr client::get_rpc_server() const { return my->_rpc_server; }
    bts::net::node_ptr client::get_node()const { return my->_p2p_node; }

    fc::variant_object version_info()
    {
      fc::mutable_variant_object info;
      info["bitshares_toolkit_revision"]     = bts::utilities::git_revision_sha;
      info["bitshares_toolkit_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(bts::utilities::git_revision_unix_timestamp));
      info["fc_revision"]                    = fc::git_revision_sha;
      info["fc_revision_age"]                = fc::get_approximate_relative_time_string(fc::time_point_sec(fc::git_revision_unix_timestamp));
      info["compile_date"]                   = "compiled on " __DATE__ " at " __TIME__;
      return info;
    }

    bts::blockchain::transaction_id_type detail::client_impl::network_broadcast_transaction(const bts::blockchain::signed_transaction& transaction_to_broadcast)
    {
      ilog("broadcasting transaction: ${id} ", ("id", transaction_to_broadcast.id()));

      // p2p doesn't send messages back to the originator
      on_new_transaction(transaction_to_broadcast);
      _p2p_node->broadcast(trx_message(transaction_to_broadcast));
      return transaction_to_broadcast.id();
    }

    //JSON-RPC Method Implementations START
    bts::blockchain::block_id_type detail::client_impl::blockchain_get_blockhash(uint32_t block_number) const
    {
      return _chain_db->get_block(block_number).id();
    }

    uint32_t detail::client_impl::blockchain_get_blockcount() const
    {
      return _chain_db->get_head_block_num();
    }

    void detail::client_impl::wallet_open_file(const fc::path& wallet_filename)
    {
      _wallet->open_file( wallet_filename );
    }

    void detail::client_impl::wallet_open(const string& wallet_name)
    {
      _wallet->open(fc::trim(wallet_name));
    }

    fc::optional<variant> detail::client_impl::wallet_get_setting(const string& name)
    {
        return _wallet->get_setting( name );
    }
    
    void detail::client_impl::wallet_set_setting(const string& name, const variant& value)
    {
        _wallet->set_setting( name, value );
    }


    void detail::client_impl::wallet_create(const string& wallet_name, const string& password, const string& brain_key)
    {
       string trimmed_name = fc::trim(wallet_name);
       if( brain_key.size() && brain_key.size() < BTS_WALLET_MIN_BRAINKEY_LENGTH ) FC_CAPTURE_AND_THROW( brain_key_too_short );
       if( password.size() < BTS_WALLET_MIN_PASSWORD_LENGTH ) FC_CAPTURE_AND_THROW( password_too_short );
       if( trimmed_name.size() == 0 ) FC_CAPTURE_AND_THROW( fc::invalid_arg_exception, (trimmed_name) );
      _wallet->create(trimmed_name,password, brain_key );
    }

    fc::optional<string> detail::client_impl::wallet_get_name() const
    {
      return _wallet->is_open() ? _wallet->get_wallet_name() : fc::optional<string>();
    }

    void detail::client_impl::wallet_close()
    {
      _wallet->close();
    }

    void detail::client_impl::wallet_export_to_json(const fc::path& json_filename)const
    {
      _wallet->export_to_json(json_filename);
    }

    void detail::client_impl::wallet_create_from_json(const fc::path& json_filename, const string& wallet_name, const string& imported_wallet_passphrase)
    {
      _wallet->create_from_json(json_filename, wallet_name, imported_wallet_passphrase);
    }

    void detail::client_impl::wallet_lock()
    {
      _wallet->lock();
    }

    void detail::client_impl::wallet_unlock(const fc::microseconds& timeout, const string& password)
    {
      _wallet->unlock(password,timeout);
    }
    void detail::client_impl::wallet_change_passphrase(const string& new_password)
    {
      _wallet->change_passphrase(new_password);
    }

    void detail::client_impl::wallet_clear_pending_transactions()
    {
        _wallet->clear_pending_transactions();
    }

    vector<signed_transaction> detail::client_impl::wallet_multipart_transfer(double amount_to_transfer, 
                                                       const string& asset_symbol, 
                                                       const string& from_account_name, 
                                                       const string& to_account_name, 
                                                       const string& memo_message)
    {
         auto trxs = _wallet->multipart_transfer( amount_to_transfer, asset_symbol, 
                                                  from_account_name, to_account_name, 
                                                  memo_message, true );
         for( auto trx : trxs )
         {
            network_broadcast_transaction( trx );
         }

         return trxs;
    }

    signed_transaction detail::client_impl::wallet_transfer(double amount_to_transfer, 
                                                       const string& asset_symbol, 
                                                       const string& from_account_name, 
                                                       const string& to_account_name, 
                                                       const string& memo_message)
    {
         auto trx = _wallet->transfer_asset( amount_to_transfer, asset_symbol, 
                                                  from_account_name, to_account_name, 
                                                  memo_message, true );

         network_broadcast_transaction( trx );

         return trx;
    }


    bts::wallet::pretty_transaction detail::client_impl::wallet_get_pretty_transaction(const bts::blockchain::signed_transaction& transaction) const
    {
      return _wallet->to_pretty_trx(wallet_transaction_record(transaction));
    }


    bts::blockchain::signed_transaction detail::client_impl::wallet_asset_create(const string& symbol, 
                                                                    const string& asset_name, 
                                                                    const string& issuer_name, 
                                                                    const string& description /* = fc::variant("").as<string>() */, 
                                                                    const variant& data /* = fc::variant("").as<fc::variant_object>() */, 
                                                                    int64_t maximum_share_supply /* = fc::variant("1000000000000000").as<int64_t>() */,
                                                                    int64_t precision /* = 0 */)
    {
      generate_transaction_flag flag = sign_and_broadcast;
      bool sign = flag != do_not_sign;
      auto create_asset_trx = 
        _wallet->create_asset(symbol, asset_name, description, data, issuer_name, maximum_share_supply, precision, sign);
      if (flag == sign_and_broadcast)
          network_broadcast_transaction(create_asset_trx);
      return create_asset_trx;
    }


    signed_transaction  detail::client_impl::wallet_asset_issue(double real_amount,
                                                   const string& symbol,
                                                   const string& to_account_name,
                                                   const string& memo_message
                                                   )
    {
      //rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast;
      //bool sign = (flag != client::do_not_sign);
      auto issue_asset_trx = _wallet->issue_asset(real_amount,symbol,to_account_name, memo_message, true);
      //if (flag == client::sign_and_broadcast)
          network_broadcast_transaction(issue_asset_trx);
      return issue_asset_trx;
    }



    signed_transaction detail::client_impl::wallet_submit_proposal( const string& delegate_account_name,
                                                       const string& subject,
                                                       const string& body,
                                                       const string& proposal_type,
                                                       const fc::variant& json_data)
    {
      try {
        generate_transaction_flag flag = bts::rpc::sign_and_broadcast;
        bool sign = (flag != bts::rpc::do_not_sign);
        auto trx = _wallet->create_proposal(delegate_account_name, subject, body, proposal_type, json_data, sign);
        if (flag == bts::rpc::sign_and_broadcast)
        {
            network_broadcast_transaction(trx);
        }
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_account_name", delegate_account_name)("subject", subject))
    }


    signed_transaction detail::client_impl::wallet_vote_proposal(const string& name,
                                                    const proposal_id_type& proposal_id,
                                                    const proposal_vote::vote_type& vote,
                                                    const string& message )
    {
      try {
        generate_transaction_flag flag = bts::rpc::sign_and_broadcast;
        bool sign = (flag != bts::rpc::do_not_sign);
        auto trx = _wallet->vote_proposal(name, proposal_id, vote, message, sign);
        if (flag == bts::rpc::sign_and_broadcast)
        {
            network_broadcast_transaction(trx);
        }
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("name", name)("proposal_id", proposal_id)("vote", vote))
    }


    vector<string> detail::client_impl::wallet_list() const
    {
      return _wallet->list();  
    }

    vector<wallet_account_record> detail::client_impl::wallet_list_accounts() const
    {
      return _wallet->list_accounts();
    }
    vector<wallet_account_record> detail::client_impl::wallet_list_my_accounts() const
    {
      return _wallet->list_my_accounts();
    }

    void detail::client_impl::wallet_remove_contact_account(const string& account_name)
    {
      _wallet->remove_contact_account( account_name );
    }

    void detail::client_impl::wallet_account_rename(const string& current_account_name,
                                       const string& new_account_name)
    {
      _wallet->rename_account(current_account_name, new_account_name);
    }


    wallet_account_record detail::client_impl::wallet_get_account(const string& account_name) const
    { try {
      auto opt_account = _wallet->get_account(account_name);
      if( opt_account.valid() )
         return *opt_account;
      FC_ASSERT(false, "Invalid Account Name: ${account_name}", ("account_name",account_name) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


    vector<pretty_transaction> detail::client_impl::wallet_account_transaction_history(const string& account) 
    {
      return _wallet->get_pretty_transaction_history(account);
    }


    oaccount_record detail::client_impl::blockchain_get_account_record(const string& name) const
    {
      return _chain_db->get_account_record(name);
    }

    oaccount_record detail::client_impl::blockchain_get_account_record_by_id(const name_id_type& name_id) const
    {
      return _chain_db->get_account_record(name_id);
    }

    oasset_record detail::client_impl::blockchain_get_asset_record(const string& symbol) const
    {
      return _chain_db->get_asset_record(symbol);
    }

    oasset_record detail::client_impl::blockchain_get_asset_record_by_id(const asset_id_type& asset_id) const
    {
      return _chain_db->get_asset_record(asset_id);
    }

    void detail::client_impl::wallet_set_delegate_trust_level( const string& delegate_name, 
                                                  int32_t user_trust_level)
    {
      try {
        auto account_record = _chain_db->get_account_record(delegate_name);
        FC_ASSERT(account_record.valid(), "delegate ${d} does not exist", ("d", delegate_name));
        FC_ASSERT(account_record->is_delegate(), "${d} is not a delegate", ("d", delegate_name));

        _wallet->set_delegate_trust_level(delegate_name, user_trust_level);
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_name", delegate_name)("user_trust_level", user_trust_level))
    }

    /*
    bts::wallet::delegate_trust_status client::wallet_get_delegate_trust_status(const string& delegate_name) const
    {
      try {
        auto account_record = _chain_db->get_account_record(delegate_name);
        FC_ASSERT(account_record.valid(), "delegate ${d} does not exist", ("d", delegate_name));
        FC_ASSERT(account_record->is_delegate(), "${d} is not a delegate", ("d", delegate_name));

        return _wallet->get_delegate_trust_status(delegate_name);
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_name", delegate_name))
    }

    map<string, bts::wallet::delegate_trust_status> client::wallet_list_delegate_trust_status() const
    {
      return _wallet->list_delegate_trust_status();
    }
    */


    otransaction_record detail::client_impl::blockchain_get_transaction(const string& transaction_id, bool exact ) const
    {
      auto id = variant( transaction_id ).as<transaction_id_type>();
      return _chain_db->get_transaction(id, exact);
    }

    digest_block detail::client_impl::blockchain_get_block(const block_id_type& block_id) const
    {
      return _chain_db->get_block_digest(block_id);
    }

    digest_block detail::client_impl::blockchain_get_block_by_number(uint32_t block_number) const
    {
      return _chain_db->get_block_digest(block_number);
    }

    void detail::client_impl::wallet_import_bitcoin(const fc::path& filename,
                                                    const string& passphrase,
                                                    const string& account_name )
    {
      try
      {
          _wallet->import_bitcoin_wallet(filename, "", account_name);
          return;
      }
      catch( const fc::exception& e )
      {
          ilog( "import_bitcoin_wallet failed with empty password: ${e}", ("e",e.to_detail_string() ) );
      }

      _wallet->import_bitcoin_wallet(filename, passphrase, account_name);
    }
    void detail::client_impl::wallet_import_multibit(const fc::path& filename,
                                                    const string& passphrase,
                                                    const string& account_name )
    {
      _wallet->import_multibit_wallet(filename, passphrase, account_name);
    }
    void detail::client_impl::wallet_import_electrum(const fc::path& filename,
                                                    const string& passphrase,
                                                    const string& account_name )
    {
      _wallet->import_electrum_wallet(filename, passphrase, account_name);
    }
    void detail::client_impl::wallet_import_armory(const fc::path& filename,
                                                    const string& passphrase,
                                                    const string& account_name )
    {
      _wallet->import_armory_wallet(filename, passphrase, account_name);
    }
    
    void detail::client_impl::wallet_import_keyhotee(const string& firstname,
                                                     const string& middlename,
                                                     const string& lastname,
                                                     const string& brainkey,
                                                     const string& keyhoteeid)
    {
        _wallet->import_keyhotee(firstname, middlename, lastname, brainkey, keyhoteeid);
    }

    void detail::client_impl::wallet_import_private_key(const string& wif_key_to_import, 
                                           const string& account_name,
                                           bool create_account,
                                           bool wallet_rescan_blockchain)
    {
      _wallet->import_wif_private_key(wif_key_to_import, account_name, create_account );
      if (wallet_rescan_blockchain)
        _wallet->scan_chain(0);
    }
   
    string detail::client_impl::wallet_dump_private_key(const std::string& address_or_public_key)
    {
      try {
          // TODO is_valid should not throw, should return false...
         bool is_address = true;
         try { 
            is_address = address::is_valid(address_or_public_key);
         }
         catch (...)
         {
            is_address = false;
         }
         if (is_address)
         {
             address addr = address(address_or_public_key);
             auto wif_private_key = bts::utilities::key_to_wif( _wallet->get_private_key(addr) );
             return wif_private_key;
         } 
         else
         {
              public_key_type pubkey = public_key_type(address_or_public_key);
              address addr = address(pubkey);
              auto wif_private_key = bts::utilities::key_to_wif( _wallet->get_private_key(addr) );
              return wif_private_key;
         }
    } FC_CAPTURE_AND_RETHROW( (address_or_public_key) ) }


    vector<account_record> detail::client_impl::blockchain_list_registered_accounts( const string& first, int32_t count) const
    {
      return _chain_db->get_accounts(first, count);
    }

    vector<asset_record> detail::client_impl::blockchain_list_registered_assets( const string& first, int32_t count) const
    {
      return _chain_db->get_assets(first, count);
    }

    vector<account_record> detail::client_impl::blockchain_list_delegates(uint32_t first, uint32_t count) const
    {
      auto delegates = _chain_db->get_delegates_by_vote(first, count);
      vector<account_record> delegate_records;
      delegate_records.reserve( delegates.size() );
      for( auto delegate_id : delegates )
        delegate_records.push_back( *_chain_db->get_account_record( delegate_id ) );
      return delegate_records;
    }
    
    std::vector<fc::variant_object> detail::client_impl::network_get_peer_info() const
    {
      std::vector<fc::variant_object> results;
      vector<bts::net::peer_status> peer_statuses = _p2p_node->get_connected_peers();
      for (const bts::net::peer_status& peer_status : peer_statuses)
        results.push_back(peer_status.info);
      return results;
    }

    void detail::client_impl::network_set_allowed_peers(const vector<bts::net::node_id_t>& allowed_peers)
    {
      _p2p_node->set_allowed_peers( allowed_peers );
    }

    void detail::client_impl::network_set_advanced_node_parameters(const fc::variant_object& params)
    {
      _p2p_node->set_advanced_node_parameters( params );
    }

    fc::variant_object detail::client_impl::network_get_advanced_node_parameters() const
    {
      return _p2p_node->get_advanced_node_parameters();
    }

    void detail::client_impl::network_add_node(const fc::ip::endpoint& node, const string& command)
    {
      if (_p2p_node)
      {
        if (command == "add")
          _p2p_node->add_node( node );
      }
    }
    
    address detail::client_impl::bitcoin_getaccountaddress(const string& account_name)
    {
       try {
          FC_ASSERT( _wallet->is_valid_account_name( account_name ) );
          if ( _wallet->is_valid_account( account_name ) )
          {
             return address( _wallet->get_account_public_key( account_name ) );
          }
          
          return _wallet->get_new_address( account_name );
       } FC_CAPTURE_AND_RETHROW( (account_name) ) }
    
    bts::blockchain::account_record detail::client_impl::bitcoin_getaccount(const address& account_address)
    {
        try {
            auto opt_account = _wallet->get_account_record(account_address);
            if( opt_account.valid() )
                return *opt_account;
            FC_ASSERT(false, "Invalid Account Key: ${account_address}", ("account_address",account_address) );
        } FC_CAPTURE_AND_RETHROW( (account_address) ) }
    
    string detail::client_impl::bitcoin_dumpprivkey(const address& account_address){
        try {
            auto wif_private_key = bts::utilities::key_to_wif( _wallet->get_private_key(account_address) );
            return wif_private_key;
        } FC_CAPTURE_AND_RETHROW( (account_address) ) }

    void detail::client_impl::bitcoin_encryptwallet(const string& passphrase)
    {
        wallet_change_passphrase(passphrase);
    }

    void detail::client_impl::bitcoin_addnode(const fc::ip::endpoint& node, const string& command)
    {
        network_add_node(node, command);
    }

    void detail::client_impl::bitcoin_backupwallet(const fc::path& destination) const
    {
        wallet_export_to_json(destination);
    }

    std::vector<address> detail::client_impl::bitcoin_getaddressesbyaccount(const string& account_name)
    { try {
       std::vector<address> addresses;
       auto public_keys = _wallet->get_public_keys_in_account(account_name);

       addresses.reserve(public_keys.size());

       for ( auto key : public_keys )
           addresses.push_back( address(key) );
       return addresses;
    } FC_CAPTURE_AND_RETHROW( (account_name) ) }

    int64_t detail::client_impl::bitcoin_getbalance(const string& account_name)
    { try {

       auto balances = _wallet->get_account_balances();
       auto itr = balances.find( account_name );
       if( itr != balances.end() )
       {
          auto bitr = itr->second.find( BTS_BLOCKCHAIN_SYMBOL );
          if( bitr != itr->second.end() )
             return bitr->second;
       }
       return 0;

    } FC_CAPTURE_AND_RETHROW( (account_name) ) }


    bts::blockchain::digest_block detail::client_impl::bitcoin_getblock(const bts::blockchain::block_id_type& block_id) const
    {
       return blockchain_get_block(block_id);
    }

    uint32_t detail::client_impl::bitcoin_getblockcount() const
    {
       return blockchain_get_blockcount();
    }

    bts::blockchain::block_id_type detail::client_impl::bitcoin_getblockhash(uint32_t block_number) const
    {
       return blockchain_get_blockhash(block_number);
    }

    uint32_t detail::client_impl::bitcoin_getconnectioncount() const
    {
       return network_get_connection_count();
    }

    fc::variant_object detail::client_impl::bitcoin_getinfo() const
    {
       return get_info();
    }

    bts::blockchain::address detail::client_impl::bitcoin_getnewaddress(const string& account_name)
    {
       return _wallet->get_new_address( account_name );
    }

    int64_t detail::client_impl::bitcoin_getreceivedbyaddress(const address& account_address)
    {
       try {
          auto balance = _chain_db->get_balance_record( account_address );
          if (balance.valid() && balance->asset_id() == 0)
          {
             return balance->balance;
          }
          else
          {
             return 0;
          }
       } FC_CAPTURE_AND_RETHROW( (account_address) ) }

    void detail::client_impl::bitcoin_importprivkey(const string& wif_key, const string& account_name, bool rescan)
    {
       wallet_import_private_key(wif_key, account_name, rescan);
    }
    
    std::unordered_map< string, bts::blockchain::share_type > detail::client_impl::bitcoin_listaccounts()
    {
       auto account_blances = _wallet->get_account_balances();
       
       std::unordered_map< string, bts::blockchain::share_type > account_bts_balances;
       for ( auto account_blance : account_blances )
       {
          if ( account_blance.second.find( BTS_BLOCKCHAIN_SYMBOL ) != account_blance.second.end() )
          {
             account_bts_balances[ account_blance.first ] = account_blance.second[ BTS_BLOCKCHAIN_SYMBOL ];
          }
       }
       
       return account_bts_balances;
    }

    std::vector<bts::wallet::pretty_transaction> detail::client_impl::bitcoin_listtransactions(const string& account_name, uint64_t count, uint64_t from)
    {
       try {
          auto trx_history = wallet_account_transaction_history( account_name );
          trx_history.reserve(trx_history.size());
          
          std::vector<bts::wallet::pretty_transaction> trxs;
          
          uint64_t index = 0;
          for ( auto trx : trx_history )
          {
             if ( index >= from )
             {
                trxs.push_back( trx );
             }
             
             if ( trxs.size() >= count )
             {
                break;
             }
             index ++;
          }
          return trxs;
       } FC_CAPTURE_AND_RETHROW( (account_name)(count)(from) ) }

    bts::blockchain::transaction_id_type detail::client_impl::bitcoin_sendfrom(const string& fromaccount, const address& toaddress, int64_t amount, const string& comment)
    {
      
       try {
          auto trx = _wallet->transfer_asset_to_address( amount, BTS_ADDRESS_PREFIX,
                                                         fromaccount, toaddress,
                                                         comment, true );
          
          network_broadcast_transaction( trx );
          
          return trx.id();
       } FC_CAPTURE_AND_RETHROW( (fromaccount)(toaddress)(amount)(comment) ) }

    bts::blockchain::transaction_id_type detail::client_impl::bitcoin_sendmany(const string& fromaccount, const std::unordered_map< address, int64_t >& to_address_amounts, const string& comment)
    {
       try {
          std::unordered_map< address, double > to_address_amount_map;
          for ( auto address_amount : to_address_amounts )
          {
             to_address_amount_map[address_amount.first] = address_amount.second;
          }
          
          auto trx = _wallet->transfer_asset_to_many_address(BTS_ADDRESS_PREFIX, fromaccount, to_address_amount_map, comment, true);
          
          network_broadcast_transaction(trx);
          
          return trx.id();
       } FC_CAPTURE_AND_RETHROW( (fromaccount)(to_address_amounts)(comment) ) }
   
    bts::blockchain::transaction_id_type detail::client_impl::bitcoin_sendtoaddress(const address& address, int64_t amount, const string& comment)
    {
       FC_ASSERT(false, "Do not support send to address from multi account yet, if you need, please contact the dev.");
    }

    void detail::client_impl::bitcoin_settrxfee(int64_t amount)
    {
       _wallet->set_priority_fee( amount );
    }

    string detail::client_impl::bitcoin_signmessage(const address& address_to_sign_with, const string& message)
    { try {
       auto private_key = _wallet->get_private_key(address_to_sign_with);
       
       auto sig = private_key.sign_compact( fc::sha256::hash( BTS_MESSAGE_MAGIC + message ) );
       
       return fc::to_base58( (char *)sig.data, sizeof(sig) );
    
    } FC_CAPTURE_AND_RETHROW( (address_to_sign_with)(message) ) }
   
    bool detail::client_impl::bitcoin_verifymessage(const address& address_to_verify_with, const string& signature, const string& message)
    { try {
       fc::ecc::compact_signature sig;
       fc::from_base58(signature, (char*)sig.data, sizeof(sig));
       
       return address_to_verify_with ==  address(fc::ecc::public_key(sig, fc::sha256::hash( BTS_MESSAGE_MAGIC + message)));
    } FC_CAPTURE_AND_RETHROW( (address_to_verify_with)(signature)(message) ) }

    fc::variant detail::client_impl::bitcoin_validateaddress(const address& address_to_validate )
    { try {
       fc::mutable_variant_object obj("address",address_to_validate);
       
       auto opt_account = _wallet->get_account_record(address_to_validate);
       if ( opt_account.valid() )
       {
          obj( "account", *opt_account );
       }
       else {
          auto opt_register_account = _chain_db->get_account_record( address_to_validate );
          if ( opt_register_account.valid() )
          {
             obj( "account", *opt_register_account );
          }
       }
       
       obj( "ismine", _wallet->is_receive_address( address_to_validate ) );
       obj( "isvalid", address::is_valid( string(address_to_validate) ) );
       
       return obj;
    } FC_CAPTURE_AND_RETHROW( (address_to_validate) ) }

    void detail::client_impl::bitcoin_walletlock()
    {
        wallet_lock();
    }

    void detail::client_impl::bitcoin_walletpassphrase(const string& passphrase, const fc::microseconds& timeout)
    {
        wallet_unlock(timeout, passphrase);
    }

    void detail::client_impl::bitcoin_walletpassphrasechange(const string& oldpassphrase, const string& newpassphrase)
    {
       _wallet->unlock(oldpassphrase);
       _wallet->change_passphrase(newpassphrase);
    }

    void detail::client_impl::stop()
    {
      _rpc_server->shutdown_rpc_server();
    }

    uint32_t detail::client_impl::network_get_connection_count() const
    {
      return _p2p_node->get_connection_count();
    }

    bts::net::message_propagation_data detail::client_impl::network_get_transaction_propagation_data(const transaction_id_type& transaction_id)
    {
      return _p2p_node->get_transaction_propagation_data(transaction_id);
      FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
    }

    bts::net::message_propagation_data detail::client_impl::network_get_block_propagation_data(const block_id_type& block_id)
    {
      return _p2p_node->get_block_propagation_data(block_id);
      FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
    }


    //JSON-RPC Method Implementations END


    string extract_commands_from_log_stream(std::istream& log_stream)
    {
      string command_list;
      string line;
      while (std::getline(log_stream,line))
      {
        //if line begins with a prompt, add to input buffer
        size_t prompt_position = line.find(CLI_PROMPT_SUFFIX);
        if (prompt_position != string::npos )
        { 
          size_t command_start_position = prompt_position + strlen(CLI_PROMPT_SUFFIX);
          command_list += line.substr(command_start_position);
          command_list += "\n";
        }
      }
      return command_list;
    }

    string extract_commands_from_log_file(fc::path test_file)
    {
      std::ifstream test_input(test_file.string());
      return extract_commands_from_log_stream(test_input);
    }


    //RPC server and CLI configuration rules:
    //if daemon mode requested
    //  start RPC server only (no CLI input)
    //else
    //  start RPC server if requested
    //  start CLI
    //  if input log
    //    cli.processs_commands in input log
    //    wait till finished
    //  set input stream to cin
    //  cli.process_commands from cin
    //  wait till finished
    void client::configure_from_command_line(int argc, char** argv)
    {
      if( argc == 0 && argv == nullptr )
      {
        my->_cli = new bts::cli::cli( this->shared_from_this(), nullptr, &std::cout );
        return;
      }
      // parse command-line options
      auto option_variables = parse_option_variables(argc,argv);

      fc::path datadir = bts::client::get_data_dir(option_variables);


      auto cfg   = load_config(datadir);
      //std::cout << fc::json::to_pretty_string( cfg ) <<"\n";
      fc::configure_logging( cfg.logging );

      load_and_configure_chain_database(datadir, option_variables);

      fc::optional<fc::path> genesis_file_path;
      if (option_variables.count("genesis-config"))
        genesis_file_path = option_variables["genesis-config"].as<string>();

      this->open( datadir, genesis_file_path );
      this->run_delegate();

      my->configure_rpc_server(cfg,option_variables);

      this->configure( datadir );

      if (option_variables.count("maximum-number-of-connections"))
      {
        fc::mutable_variant_object params;
        params["maximum_number_of_connections"] = option_variables["maximum-number-of-connections"].as<uint16_t>();
        this->network_set_advanced_node_parameters(params);
      }

      if (option_variables.count("p2p-port"))
      {
        uint16_t p2pport = option_variables["p2p-port"].as<uint16_t>();
        listen_on_port(p2pport, option_variables.count("p2p-port") != 0);
      }
      // else we use the default set in bts::net::node

      // start listening.  this just finds a port and binds it, it doesn't start
      // accepting connections until connect_to_p2p_network()
      listen_to_p2p_network();

      if( option_variables["upnp"].as<bool>() )
      {
        std::cout << "Attempting to map P2P port " << get_p2p_listening_endpoint().port() << " with UPNP...\n";
        my->_upnp_service = std::unique_ptr<bts::net::upnp_service>(new bts::net::upnp_service);
        my->_upnp_service->map_port( get_p2p_listening_endpoint().port() );
        fc::usleep( fc::seconds(3) );
      }

      if (option_variables.count("total-bandwidth-limit"))
      {
        this->get_node()->set_total_bandwidth_limit(option_variables["total-bandwidth-limit"].as<uint32_t>(), 
                                                    option_variables["total-bandwidth-limit"].as<uint32_t>());
      }

      if (option_variables.count("clear-peer-database"))
      {
        std::cout << "Erasing old peer database\n";
        this->get_node()->clear_peer_database();
      }

      // fire up the p2p , 
      connect_to_p2p_network();
      fc::ip::endpoint actual_p2p_endpoint = this->get_p2p_listening_endpoint();
      std::cout << "Listening for P2P connections on ";
      if (actual_p2p_endpoint.get_address() == fc::ip::address())
        std::cout << "port " << actual_p2p_endpoint.port();
      else
        std::cout << (string)actual_p2p_endpoint;
      if (option_variables.count("p2p-port"))
      {
        uint16_t p2p_port = option_variables["p2p-port"].as<uint16_t>();
        if (p2p_port != 0 && p2p_port != actual_p2p_endpoint.port())
          std::cout << " (unable to bind to the desired port " << p2p_port << ")";
      }
      std::cout << "\n";


      if (option_variables.count("connect-to"))
      {
        std::vector<string> hosts = option_variables["connect-to"].as<std::vector<string>>();
        for( auto peer : hosts )
          this->connect_to_peer( peer );
      }
      else if (!option_variables.count("disable-default-peers"))
      {
        for (string default_peer : cfg.default_peers)
          this->connect_to_peer(default_peer);
      }

      if( option_variables.count("daemon") || cfg.ignore_console )
      {
        std::cout << "Running in daemon mode, ignoring console\n";
        my->_cli = new bts::cli::cli( this->shared_from_this(), nullptr, &std::cout );
        my->_cli->set_daemon_mode(true);
      }
      else //we will accept input from the console
      {
        //if user wants us to execute a command script log for the CLI,
        //  extract the commands and put them in a temporary input stream to pass to the CLI
        
        if (option_variables.count("input-log"))
        {
          string input_commands = extract_commands_from_log_file(option_variables["input-log"].as<string>());
          my->_command_script_holder.reset(new std::stringstream(input_commands));
        }

    #if 1
        // tee cli output to the console and a log file
        fc::path console_log_file = datadir / "console.log";
        my->_console_log.open(console_log_file.string());
        my->_tee_device.reset(new TeeDevice(std::cout, my->_console_log));; 
        my->_tee_stream.reset(new TeeStream(*my->_tee_device.get()));
        //force flushing to console and log file whenever cin input is required
        std::cin.tie( my->_tee_stream.get() );
        

        my->_cli = new bts::cli::cli( this->shared_from_this(), my->_command_script_holder.get(), my->_tee_stream.get() );
        //echo command input to the log file
        my->_cli->set_input_stream_log(my->_console_log);
    #else
        //don't create a log file, just output to console
        my->_cli = new bts::cli::cli( this->shared_from_this(), my->_command_script_holder.get(), &std::cout );
    #endif
      } //end else we will accept input from the console

    }


    fc::future<void> client::start()
    {
      return fc::async( [=](){ my->start(); } );
    }

    void client::run_delegate( )
    {
      my->_delegate_loop_complete = fc::async( [=](){ my->delegate_loop(); } );
    }

    bool client::is_connected() const
    {
      return my->_p2p_node->is_connected();
    }

    bts::net::node_id_t client::get_node_id() const
    {
      return my->_p2p_node->get_node_id();
    }

    void client::listen_on_port(uint16_t port_to_listen, bool wait_if_not_available)
    {
        my->_p2p_node->listen_on_port(port_to_listen, wait_if_not_available);
    }
    
    void client::configure(const fc::path& configuration_directory)
    {
      my->_data_dir = configuration_directory;
      my->_p2p_node->load_configuration( my->_data_dir );
    }

    void client::init_cli()
    {
      if( !my->_cli )
         my->_cli = new bts::cli::cli( this->shared_from_this(), nullptr, &std::cout );
    }

    void client::set_daemon_mode(bool daemon_mode) 
    { 
      my->_cli->set_daemon_mode(daemon_mode); 
    }

    fc::path client::get_data_dir()const
    {
       return my->_data_dir;
    }
    void client::connect_to_peer(const string& remote_endpoint)

    {
        std::cout << "Attempting to connect to peer " << remote_endpoint << "\n";
        fc::ip::endpoint ep;
        try {
            ep = fc::ip::endpoint::from_string(remote_endpoint.c_str());
        } catch (...) {
            auto pos = remote_endpoint.find(':');
            uint16_t port = boost::lexical_cast<uint16_t>( remote_endpoint.substr( pos+1, remote_endpoint.size() ) );
            string hostname = remote_endpoint.substr( 0, pos );
            auto eps = fc::resolve(hostname, port);
            if ( eps.size() > 0 )
            {
                ep = eps.back();
            }
            else
            {
                FC_THROW_EXCEPTION(fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname));
            }
        }
        my->_p2p_node->connect_to(ep);
    }

    void client::listen_to_p2p_network()
    {
      my->_p2p_node->listen_to_p2p_network();
    }

    void client::connect_to_p2p_network()
    {
      bts::net::item_id head_item_id;
      head_item_id.item_type = bts::client::block_message_type;
      uint32_t last_block_num = my->_chain_db->get_head_block_num();
      if (last_block_num == (uint32_t)-1)
        head_item_id.item_hash = bts::net::item_hash_t();
      else
        head_item_id.item_hash = my->_chain_db->get_head_block_id();
      my->_p2p_node->sync_from(head_item_id);
      my->_p2p_node->connect_to_p2p_network();
    }

    fc::ip::endpoint client::get_p2p_listening_endpoint() const
    {
      return my->_p2p_node->get_actual_listening_endpoint();
    }

#ifndef NDEBUG
    bool client::handle_message(const bts::net::message& message, bool sync_mode)
    {
      return my->handle_message(message, sync_mode);
    }
#endif

    fc::sha256 client_notification::digest()const
    {
      fc::sha256::encoder enc;
      fc::raw::pack(enc, *this);
      return enc.result();
    }

    void client_notification::sign(const fc::ecc::private_key& key)
    {
      signature = key.sign_compact(digest());
    }

    fc::ecc::public_key client_notification::signee() const
    {
      return fc::ecc::public_key(signature, digest());
    }


   /**
    * Detail Implementation 
    */
   namespace detail  {


    void client_impl::wallet_add_contact_account( const string& account_name, 
                                             const public_key_type& contact_key )
    {
       _wallet->add_contact_account( account_name, contact_key );
    }


    public_key_type client_impl::wallet_account_create( const string& account_name,
                                                   const variant& private_data )
    {
       ilog( "CLIENT: creating account '${account_name}'", ("account_name",account_name) );
       return _wallet->create_account( account_name, private_data );
    }

    fc::variant_object client_impl::about() const
    {
      return bts::client::version_info();
    }

    string client_impl::help(const string& command_name) const
    {
      return _rpc_server->help(command_name);
    }

    method_map_type client_impl::meta_help() const
    {
      return _rpc_server->meta_help();
    }
    

    variant_object client_impl::blockchain_get_config() const
    {
       fc::mutable_variant_object info;
       info["blockchain_id"]                        = _chain_db->chain_id();
       info["block_interval"]                       = BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
       info["max_block_size"]                       = BTS_BLOCKCHAIN_MAX_BLOCK_SIZE;
       info["target_block_size"]                    = BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE;
       info["block_reward"]                         = _chain_db->to_pretty_asset( asset(BTS_BLOCKCHAIN_BLOCK_REWARD, 0) );
       info["inactivity_fee_apr"]                   = BTS_BLOCKCHAIN_INACTIVE_FEE_APR;
       info["max_blockchain_size"]                  = BTS_BLOCKCHAIN_MAX_SIZE;
       info["symbol"]                               = BTS_BLOCKCHAIN_SYMBOL;
       info["name"]                                 = BTS_BLOCKCHAIN_NAME;
       info["version"]                              = BTS_BLOCKCHAIN_VERSION;
       info["address_prefix"]                       = BTS_ADDRESS_PREFIX;

       info["min_block_fee"]                        = double( BTS_BLOCKCHAIN_MIN_FEE ) / 1000;

       info["delegate_num"]                         = BTS_BLOCKCHAIN_NUM_DELEGATES;
       info["delegate_reg_fee"]                     = BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE;
       info["delegate_reward_min"]                  = BTS_BLOCKCHAIN_BLOCK_REWARD;


       info["name_size_max"]                        = BTS_BLOCKCHAIN_MAX_NAME_SIZE;
       info["symbol_size_max"]                      = BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE;
       info["symbol_size_min"]                      = BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE;
       info["data_size_max"]                        = BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE;
       info["asset_reg_fee"]                        = BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE;
       info["asset_shares_max"]                     = BTS_BLOCKCHAIN_MAX_SHARES;

       return info;

    }

    variant_object client_impl::get_info() const
    {
      fc::mutable_variant_object info;
      auto share_record = _chain_db->get_asset_record( BTS_ADDRESS_PREFIX );
      auto current_share_supply = share_record.valid() ? share_record->current_share_supply : 0;
      auto advanced_params = network_get_advanced_node_parameters();
      fc::variant wallet_balance_shares;

      info["blockchain_head_block_num"]                  = _chain_db->get_head_block_num();
      info["blockchain_head_block_time"]                 = _chain_db->now();
      info["blockchain_head_block_time_rel"]             = fc::get_approximate_relative_time_string(_chain_db->now(), bts::blockchain::now(), " old");
      info["blockchain_confirmation_requirement"]        = _chain_db->get_required_confirmations();
      info["blockchain_average_delegate_participation"]  = _chain_db->get_average_delegate_participation();
      info["network_num_connections"]                    = network_get_connection_count();
      info["ntp_time"]                                   = blockchain::ntp_time();
      if( blockchain::ntp_time() )
         info["ntp_error_seconds"]                       = (*blockchain::ntp_time() - fc::time_point::now()).count()/double(1000000);
      auto seconds_remaining = (_wallet->unlocked_until() - bts::blockchain::now()).count()/1000000;
      info["wallet_unlocked_seconds_remaining"]    = seconds_remaining > 0 ? seconds_remaining : 0;
      if( _wallet->next_block_production_time() != fc::time_point_sec() )
      {
        info["wallet_next_block_production_time"] = _wallet->next_block_production_time();
        info["wallet_seconds_until_next_block_production"] = (_wallet->next_block_production_time() - bts::blockchain::now()).count()/1000000;
      }
      else
      {
        info["wallet_next_block_production_time"] = variant();
        info["wallet_seconds_until_next_block_production"] = variant();
      }
      info["wallet_local_time"]                    = bts::blockchain::now();
      info["blockchain_random_seed"]               = _chain_db->get_current_random_seed();

      info["blockchain_shares"]                    = current_share_supply;

  //    info["client_httpd_port"]                    = _config.is_valid() ? _config.httpd_endpoint.port() : 0;

  //   info["client_rpc_port"]                      = _config.is_valid() ? _config.rpc_endpoint.port() : 0;

      info["network_num_connections_max"]          = advanced_params["maximum_number_of_connections"];

      info["network_protocol_version"]             = BTS_NET_PROTOCOL_VERSION;

      info["wallet_open"]                          = _wallet->is_open();

      info["wallet_unlocked_until"]                = _wallet->is_open() && _wallet->is_unlocked()
                                                  ? string( _wallet->unlocked_until() )
                                                  : "";
      info["wallet_version"]                       = BTS_WALLET_VERSION;

      return info;
    }
    void client_impl::wallet_rescan_blockchain( uint32_t start, uint32_t count) 
    { try {
       _wallet->scan_chain( start, start + count );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("start",start)("count",count) ) }

    bts::blockchain::blockchain_security_state    client_impl::blockchain_get_security_state()const
    {
        auto state = blockchain_security_state();
        auto required_confirmations = _chain_db->get_required_confirmations();
        auto participation_rate = _chain_db->get_average_delegate_participation();
        state.estimated_confirmation_seconds = required_confirmations * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
        state.participation_rate = participation_rate;
        if (required_confirmations < BTS_BLOCKCHAIN_NUM_DELEGATES / 2
            && participation_rate > 90)
        {
            state.alert_level = bts::blockchain::blockchain_security_state::green;
        } 
        else if (required_confirmations > BTS_BLOCKCHAIN_NUM_DELEGATES
                 || participation_rate < 60)
        {
            state.alert_level = bts::blockchain::blockchain_security_state::red;
        }
        else
        {
            state.alert_level = bts::blockchain::blockchain_security_state::yellow;
        }
        return state;
    }

    wallet_transaction_record client_impl::wallet_account_register( const string& account_name,
                                                        const string& pay_with_account,
                                                        const fc::variant& data,
                                                        bool as_delegate ) 
    {
      try {
        // bool sign = (flag != do_not_sign);
        auto trx = _wallet->register_account(account_name, data, as_delegate, pay_with_account, true);//sign);
        network_broadcast_transaction( trx.trx );
        /*
        if( flag == sign_and_broadcast )
        {
            network_broadcast_transaction(trx);
        }
        */
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("data", data))
    }

    variant_object client_impl::wallet_get_info()
    {
       return _wallet->get_info().get_object();
    }

    void client_impl::wallet_account_update_private_data( const string& account_to_update,
                                                          const variant& private_data )
    {
       _wallet->update_account_private_data(account_to_update, private_data); 
    }

    wallet_transaction_record client_impl::wallet_account_update_registration( const string& account_to_update,
                                                                        const string& pay_from_account,
                                                                        const variant& public_data,
                                                                        bool as_delegate )
    {
       auto trx = _wallet->update_registered_account( account_to_update, 
                                                           pay_from_account, 
                                                           public_data, 
                                                           optional<public_key_type>(),
                                                           as_delegate, true );

       network_broadcast_transaction( trx.trx );
       return trx;
    }

    fc::variant_object client_impl::network_get_info() const
    {
      return _p2p_node->network_get_info();
    }

    fc::variant_object client_impl::validate_address(const string& address) const
    {
      fc::mutable_variant_object result;
      try
      {
        bts::blockchain::public_key_type test_key(address);
        result["isvalid"] = true;
      }
      catch (const fc::exception&)
      {
        result["isvalid"] = false;
      }
      return result;
    }

    void client_impl::wait(uint32_t wait_time) const
    {
      fc::usleep(fc::seconds(wait_time));
    }


    vector<wallet_balance_record> client_impl::wallet_list_unspent_balances( const string& account_name, const string& symbol )
    {
       return _wallet->get_unspent_balances( account_name, symbol );
    }

    vector<public_key_summary> client_impl::wallet_account_list_public_keys( const string& account_name )
    {
        auto summaries = vector<public_key_summary>();
        auto keys = _wallet->get_public_keys_in_account( account_name );
        summaries.reserve( keys.size() );
        for (auto key : keys)
        {
            summaries.push_back(_wallet->get_public_key_summary( key ));
        }
        return summaries;
    }

   unordered_map<string, map<string, int64_t> >  client_impl::wallet_account_balance( const string& account_name ) 
   {
      if( account_name == string() || account_name == "*")
         return _wallet->get_account_balances();
      else
      {
         if( !_chain_db->is_valid_account_name( account_name ) )
            FC_CAPTURE_AND_THROW( invalid_account_name, (account_name) );

         if( !_wallet->is_receive_account( account_name ) )
            FC_CAPTURE_AND_THROW( unknown_receive_account, (account_name) );

         auto all = _wallet->get_account_balances();

         unordered_map<string, map<string,int64_t> > tmp;
         tmp[account_name] = all[account_name];
         tmp[account_name][BTS_BLOCKCHAIN_SYMBOL] = all[account_name][BTS_BLOCKCHAIN_SYMBOL];
         return tmp;
      }
   }

   signed_transaction client_impl::wallet_market_submit_bid( const string& from_account,
                                                             double quantity, const string& quantity_symbol,
                                                             double quote_price, const string& quote_symbol )
   {
      auto trx = _wallet->submit_bid( from_account, quantity, quantity_symbol, 
                                                    quote_price, quote_symbol, true );
      
      network_broadcast_transaction( trx );
      return trx;
   }

   signed_transaction client_impl::wallet_withdraw_delegate_pay( const string& delegate_name, 
                                                                 const string& to_account_name, 
                                                                 double amount_to_withdraw,
                                                                 const string& memo_message )
   {
      auto trx = _wallet->withdraw_delegate_pay( delegate_name, 
                                           amount_to_withdraw, 
                                           to_account_name, 
                                           memo_message, true );
      network_broadcast_transaction( trx );
      return trx;
   }
   
   void client_impl::wallet_set_priority_fee( int64_t fee )
   {
      try {
         FC_ASSERT( fee >= 0, "Priority fee should be non negative." );
         _wallet->set_priority_fee(fee);
      } FC_CAPTURE_AND_RETHROW( (fee) ) }
      
   vector<proposal_record>  client_impl::blockchain_list_proposals( uint32_t first, uint32_t count )const
   {
      return _chain_db->get_proposals( first, count );
   }
   vector<proposal_vote>    client_impl::blockchain_get_proposal_votes( const proposal_id_type& proposal_id ) const
   {
      return _chain_db->get_proposal_votes( proposal_id );
   }

   vector<market_order>    client_impl::blockchain_market_list_bids( const string& quote_symbol,
                                                                       const string& base_symbol,
                                                                       int64_t limit  )
   {
      return _chain_db->get_market_bids( quote_symbol, base_symbol, limit );
   }

   vector<market_order_status>    client_impl::wallet_market_order_list( const string& quote_symbol,
                                                                 const string& base_symbol,
                                                                 int64_t limit  )
   {
      return _wallet->get_market_orders( quote_symbol, base_symbol/*, limit*/ );
   }

   signed_transaction client_impl::wallet_market_cancel_order( const address& order_address )
   {
      auto trx = _wallet->cancel_market_order( order_address );
      network_broadcast_transaction( trx );
      return trx;
   }
   bts::wallet::wallet::account_vote_summary_type client_impl::wallet_account_vote_summary( const string& account_name )
   {
      return _wallet->get_account_vote_summary( account_name );
   }

   string client_impl::wallet_account_export_private_key( const string& account_name )
   {
      return utilities::key_to_wif( _wallet->get_account_private_key( account_name ) );
   }

   vector<transaction_record> client_impl::blockchain_get_transactions_for_block( const block_id_type& id )const
   {
      return _chain_db->get_transactions_for_block(id);
   }

   map<fc::time_point, fc::exception> client_impl::list_errors( const fc::time_point& start_time )const
   {
      map<fc::time_point, fc::exception> result;
      auto itr = _exception_db.lower_bound( start_time );
      while( itr.valid() )
      {
         result[itr.key()] = itr.value();
         ++itr;
      }
      return result;
   }
   map<fc::time_point, std::string> client_impl::list_errors_brief( const fc::time_point& start_time )const
   {
      map<fc::time_point, std::string> result;
      auto itr = _exception_db.lower_bound( start_time );
      while( itr.valid() )
      {
         result[itr.key()] = itr.value().what();
         ++itr;
      }
      return result;
   }

   void client_impl::write_errors_to_file( const string& path, const fc::time_point& start_time ) const
   {
      map<fc::time_point, fc::exception> result;
      auto itr = _exception_db.lower_bound( start_time );
      while( itr.valid() )
      {
         result[itr.key()] = itr.value();
         ++itr;
      }
      if (path != "")
      {
         std::ofstream fileout( path.c_str() );
         fileout << fc::json::to_pretty_string( result );
      }
   }

   std::string client_impl::blockchain_export_fork_graph( uint32_t start_block, uint32_t end_block, const std::string& filename )const
   {
      return _chain_db->export_fork_graph( start_block, end_block, filename );
   }

   std::vector<uint32_t> client_impl::blockchain_list_forks()const
   {
      return _chain_db->get_forks_list();
   }

   std::map<uint32_t, delegate_block_stats> client_impl::blockchain_get_delegate_block_stats( const account_id_type& delegate_id )const
   {
      return _chain_db->get_delegate_block_stats( delegate_id );
   }

   string client_impl::blockchain_get_signing_delegate( uint32_t block_number )const
   {
      auto block_header = _chain_db->get_block_header( block_number );
      auto signee = block_header.signee();
      auto delegate_record = _chain_db->get_account_record( signee );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
      return delegate_record->name;
   }

   void client_impl::wallet_enable_delegate_block_production( const string& delegate_name, bool enable )
   {
      _wallet->enable_delegate_block_production( delegate_name, enable );
   }

   vector<bts::net::potential_peer_record> client_impl::network_list_potential_peers()const
   {
        return _p2p_node->get_potential_peers();
   }

   fc::variant_object client_impl::network_get_upnp_info()const
   {
       fc::mutable_variant_object upnp_info;

       upnp_info["upnp_enabled"] = bool(_upnp_service);

       if (_upnp_service)
       {
           upnp_info["external_ip"] = fc::string(_upnp_service->external_ip());
           upnp_info["mapped_port"] = fc::variant(_upnp_service->mapped_port()).as_string();
       }

       return upnp_info;
   }

   } // namespace detail
///////////////////////////////////////////////////////////////////////////////////////////////


   bts::api::common_api* client::get_impl() const
   {
     return my.get();
   }

  bool rpc_server_config::is_valid() const
  {
    if (rpc_user.empty())
      return false;
    if (rpc_password.empty())
      return false;
    return true;
  }
   
} } // bts::client
