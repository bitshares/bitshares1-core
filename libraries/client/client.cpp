#define DEFAULT_LOGGER "client"

#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/client/notifier.hpp>
#include <bts/cli/cli.hpp>
#include <bts/net/node.hpp>
#include <bts/net/exceptions.hpp>
#include <bts/net/upnp.hpp>
#include <bts/net/peer_database.hpp>
#include <bts/net/chain_downloader.hpp>
#include <bts/net/chain_server.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/account_operations.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/git_revision.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/api/common_api.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/config.hpp>
#include <bts/mail/server.hpp>
#include <bts/mail/client.hpp>

#include <bts/client/build_info.hpp>
//#include <bts/network/node.hpp>

#include <bts/db/level_map.hpp>

#include <fc/reflect/variant.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>

#include <fc/thread/thread.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/version.hpp>

#include <openssl/opensslv.h>

#include <iostream>
#include <algorithm>
#include <fstream>
#include <iomanip>

#include <boost/lexical_cast.hpp>
using namespace boost;
using std::string;

// delegate network breaks win32
#define DISABLE_DELEGATE_NETWORK 1

#define INVOCATION_COUNTER(name) \
    static unsigned total_ ## name ## _counter = 0; \
    static unsigned active_ ## name ## _counter = 0; \
    struct name ## _invocation_logger { \
      unsigned *total; \
      unsigned *active; \
      name ## _invocation_logger(unsigned *total, unsigned *active) : \
        total(total), active(active) \
      { \
        ++*total; \
        ++*active; \
        fprintf(stderr,"NEWDEBUG: Entering %s, now %d total calls, %d active calls", #name, *total, *active); \
      } \
      ~name ## _invocation_logger() \
      { \
        --*active; \
        fprintf(stderr,"NEWDEBUG: Leaving %s, now %d total calls, %d active calls", #name, *total, *active); \
      } \
    } invocation_logger(&total_ ## name ## _counter, &active_ ## name ## _counter)

namespace bts { namespace client {

const string BTS_MESSAGE_MAGIC = "BitShares Signed Message:\n";

void print_banner();
fc::logging_config create_default_logging_config( const fc::path&, bool enable_ulog );
fc::path get_data_dir(const program_options::variables_map& option_variables);
config load_config( const fc::path& datadir, bool enable_ulog );
void  load_and_configure_chain_database(const fc::path& datadir,
                                        const program_options::variables_map& option_variables);

program_options::variables_map parse_option_variables(int argc, char** argv)
{
   // parse command-line options
   program_options::options_description option_config("Usage");
   option_config.add_options()
       ("help", "Display this help message and exit")
       ("version", "Print version information and exit")

       ("data-dir", program_options::value<string>(), "Set client data directory")

       ("genesis-config", program_options::value<string>(),
          "Generate a genesis state with the given JSON file instead of using the built-in "
          "genesis block (only accepted when the blockchain is empty)")

       ("rebuild-index", "Same as --resync-blockchain, except it preserves the raw blockchain data rather "
          "than downloading a new copy")
       ("resync-blockchain", "Delete our copy of the blockchain at startup and download a "
          "fresh copy of the entire blockchain from the network")

       ("p2p-port", program_options::value<uint16_t>(), "Set network port to listen on")
       ("accept-incoming-connections", program_options::value<bool>()->default_value(true), "Set to false to reject incoming p2p connections and only establish outbound connections")
       ("upnp", program_options::value<bool>()->default_value(true), "Enable UPNP")

       ("max-connections", program_options::value<uint16_t>(),
          "Set the maximum number of peers this node will accept at any one time")
       ("total-bandwidth-limit", program_options::value<uint32_t>()->default_value(1000000),
           "Limit total bandwidth to this many bytes per second")
       ("min-delegate-connection-count", program_options::value<uint32_t>(),
           "Override the default minimum connection count needed to produce a block")

       ("clear-peer-database", "Erase all information in the peer database")
       ("connect-to", program_options::value<std::vector<string> >(), "Set a remote host to connect to")
       ("disable-default-peers", "Disable automatic connection to default peers")
       ("disable-peer-advertising", "Don't let any peers know which other nodes we're connected to")

       ("server", "Enable JSON-RPC server")
       ("daemon", "Run in daemon mode with no CLI and start JSON-RPC server")

       ("rpcuser", program_options::value<string>(), "Set username for JSON-RPC")
       ("rpcpassword", program_options::value<string>(), "Set password for JSON-RPC")
       ("rpcport", program_options::value<uint16_t>(), "Set port to listen for JSON-RPC connections")
       ("httpdendpoint", program_options::value<string>(), "Set interface/port to listen for HTTP JSON-RPC connections")
       ("httpport", program_options::value<uint16_t>(), "Set port to listen for HTTP JSON-RPC connections")

       ("chain-server-port", program_options::value<uint16_t>(), "Run a chain server on this port")

       ("input-log", program_options::value< vector<string> >(), "Set log file with CLI commands to execute at startup")
       ("log-commands", "Log all command input and output")
       ("ulog", program_options::value<bool>()->default_value( true ), "Enable CLI user logging")

       ("growl", program_options::value<std::string>()->implicit_value("127.0.0.1"), "Send notifications about potential problems to Growl")
       ("growl-password", program_options::value<std::string>(), "Password for authenticating to a Growl server")
       ("growl-identifier", program_options::value<std::string>(), "A name displayed in growl messages to identify this bitshares_client instance")
       ;

  program_options::variables_map option_variables;
  try
  {
    program_options::store(program_options::command_line_parser(argc, argv).
      options(option_config).run(), option_variables);
    program_options::notify(option_variables);
  }
  catch (program_options::error& cmdline_error)
  {
    std::cerr << "Error: " << cmdline_error.what() << "\n";
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
  if( !fc::exists( test_file ) )
    FC_THROW( ("Unable to input-log-file: \"" + test_file.string() + "\" not found!").c_str() );
  else
    ulog("Extracting commands from input log file: ${log}",("log",test_file.string() ) );
  std::ifstream test_input(test_file.string());
  return extract_commands_from_log_stream(test_input);
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

fc::logging_config create_default_logging_config( const fc::path& data_dir, bool enable_ulog )
{
    fc::logging_config cfg;
    fc::path log_dir("logs");

    fc::file_appender::config ac;
    ac.filename             = log_dir / "default" / "default.log";
    ac.truncate             = false;
    ac.flush                = true;
    ac.rotate               = true;
    ac.rotation_interval    = fc::hours( 1 );
    ac.rotation_limit       = fc::days( 1 );
    ac.rotation_compression = true;

    std::cout << "Logging to file: " << (data_dir / ac.filename).preferred_string() << "\n";

    fc::file_appender::config ac_rpc;
    ac_rpc.filename             = log_dir / "rpc" / "rpc.log";
    ac_rpc.truncate             = false;
    ac_rpc.flush                = true;
    ac_rpc.rotate               = true;
    ac_rpc.rotation_interval    = fc::hours( 1 );
    ac_rpc.rotation_limit       = fc::days( 1 );
    ac_rpc.rotation_compression = true;

    std::cout << "Logging RPC to file: " << (data_dir / ac_rpc.filename).preferred_string() << "\n";

    fc::file_appender::config ac_blockchain;
    ac_blockchain.filename             = log_dir / "blockchain" / "blockchain.log";
    ac_blockchain.truncate             = false;
    ac_blockchain.flush                = true;
    ac_blockchain.rotate               = true;
    ac_blockchain.rotation_interval    = fc::hours( 1 );
    ac_blockchain.rotation_limit       = fc::days( 1 );
    ac_blockchain.rotation_compression = true;

    std::cout << "Logging blockchain to file: " << (data_dir / ac_blockchain.filename).preferred_string() << "\n";

    fc::file_appender::config ac_p2p;
    ac_p2p.filename             = log_dir / "p2p" / "p2p.log";
    ac_p2p.truncate             = false;
#ifdef NDEBUG
    ac_p2p.flush                = false;
#else // NDEBUG
    ac_p2p.flush                = true;
#endif // NDEBUG
    ac_p2p.rotate               = true;
    ac_p2p.rotation_interval    = fc::hours( 1 );
    ac_p2p.rotation_limit       = fc::days( 1 );
    ac_p2p.rotation_compression = true;

    std::cout << "Logging P2P to file: " << (data_dir / ac_p2p.filename).preferred_string() << "\n";

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
    cfg.appenders.push_back(fc::appender_config( "blockchain", "file", fc::variant(ac_blockchain)));
    cfg.appenders.push_back(fc::appender_config( "p2p", "file", fc::variant(ac_p2p)));

    fc::logger_config dlc;
#ifdef BTS_TEST_NETWORK
    dlc.level = fc::log_level::debug;
#else
    dlc.level = fc::log_level::warn;
#endif
    dlc.name = "default";
    dlc.appenders.push_back("default");
    dlc.appenders.push_back("p2p");
   // dlc.appenders.push_back("stderr");

    fc::logger_config dlc_client;
#ifdef BTS_TEST_NETWORK
    dlc_client.level = fc::log_level::debug;
#else
    dlc_client.level = fc::log_level::warn;
#endif
    dlc_client.name = "client";
    dlc_client.appenders.push_back("default");
    dlc_client.appenders.push_back("p2p");
   // dlc.appenders.push_back("stderr");

    fc::logger_config dlc_rpc;
#ifdef BTS_TEST_NETWORK
    dlc_rpc.level = fc::log_level::debug;
#else
    dlc_rpc.level = fc::log_level::warn;
#endif
    dlc_rpc.name = "rpc";
    dlc_rpc.appenders.push_back("rpc");

    fc::logger_config dlc_blockchain;
#ifdef BTS_TEST_NETWORK
    dlc_blockchain.level = fc::log_level::debug;
#else
    dlc_blockchain.level = fc::log_level::warn;
#endif
    dlc_blockchain.name = "blockchain";
    dlc_blockchain.appenders.push_back("blockchain");

    fc::logger_config dlc_p2p;
#ifdef BTS_TEST_NETWORK
    dlc_p2p.level = fc::log_level::debug;
#else
    dlc_p2p.level = fc::log_level::warn;
#endif
    dlc_p2p.name = "p2p";
    dlc_p2p.appenders.push_back("p2p");

    fc::logger_config dlc_user;
    if( enable_ulog ) dlc_user.level = fc::log_level::debug;
    else dlc_user.level = fc::log_level::off;
    dlc_user.name = "user";
    dlc_user.appenders.push_back("user");

    cfg.loggers.push_back(dlc);
    cfg.loggers.push_back(dlc_client);
    cfg.loggers.push_back(dlc_rpc);
    cfg.loggers.push_back(dlc_p2p);
    cfg.loggers.push_back(dlc_user);
    cfg.loggers.push_back(dlc_blockchain);

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
     auto dir_name = string( BTS_BLOCKCHAIN_NAME );
#ifdef WIN32
#elif defined( __APPLE__ )
#else
     std::string::iterator end_pos = std::remove( dir_name.begin(), dir_name.end(), ' ' );
     dir_name.erase( end_pos, dir_name.end() );
     dir_name = "." + dir_name;
#endif

#ifdef BTS_TEST_NETWORK
     dir_name += "-Test" + std::to_string( BTS_TEST_NETWORK_VERSION );
#endif

     datadir = fc::app_path() / dir_name;
   }
   return datadir;

} FC_RETHROW_EXCEPTIONS( warn, "error loading config" ) }

void load_and_configure_chain_database( const fc::path& datadir,
                                        const program_options::variables_map& option_variables)
{ try {
  //FIXME: Remove this move in a future version; this is just to bump new users up to the new version
  if (fc::exists(datadir / "chain/index/block_num_to_id_db") && fc::exists(datadir / "chain"))
      fc::rename(datadir / "chain/index/block_num_to_id_db", datadir / "chain/raw_chain/block_num_to_id_db");

  if (option_variables.count("resync-blockchain"))
  {
    std::cout << "Deleting old copy of the blockchain in: " << ( datadir / "chain" ).preferred_string() << "\n";
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
  else if (option_variables.count("rebuild-index"))
  {
      std::cout << "Clearing database index\n";
      try
      {
          fc::remove_all(datadir / "chain/index");
      }
      catch (const fc::exception& e)
      {
          std::cout << "Error while deleting database index: " << e.what() << "\n";
      }
  }
  else
  {
    std::cout << "Loading blockchain from: " << ( datadir / "chain" ).preferred_string()  << "\n";
  }

} FC_RETHROW_EXCEPTIONS( warn, "unable to open blockchain from ${data_dir}", ("data_dir",datadir/"chain") ) }

config load_config( const fc::path& datadir, bool enable_ulog )
{ try {
      fc::path config_file = datadir / "config.json";
      config cfg;
      if( fc::exists( config_file ) )
      {
         std::cout << "Loading config from file: " << config_file.preferred_string() << "\n";
         const auto default_peers = cfg.default_peers;
         cfg = fc::json::from_file( config_file ).as<config>();

         int merged_peer_count = 0;
         for( const auto& peer : default_peers )
         {
           if( std::find( cfg.default_peers.begin(), cfg.default_peers.end(), peer ) == cfg.default_peers.end() )
           {
             ++merged_peer_count;
             cfg.default_peers.push_back( peer );
           }
         }
         if( merged_peer_count > 0 )
             std::cout << "Merged " << merged_peer_count << " default peers into config.\n";
      }
      else
      {
         std::cerr << "Creating default config file at: " << config_file.preferred_string() << "\n";
         cfg.logging = create_default_logging_config( datadir, enable_ulog );
      }
      fc::json::save_to_file( cfg, config_file );

      // the logging_config may contain relative paths.  If it does, expand those to full
      // paths, relative to the data_dir
      for (fc::appender_config& appender : cfg.logging.appenders)
      {
        if (appender.type == "file")
        {
          try
          {
            fc::file_appender::config file_appender_config = appender.args.as<fc::file_appender::config>();
            if (file_appender_config.filename.is_relative())
            {
              file_appender_config.filename = fc::absolute(datadir / file_appender_config.filename);
              appender.args = fc::variant(file_appender_config);
            }
          }
          catch (const fc::exception& e)
          {
            wlog("Unexpected exception processing logging config: ${e}", ("e", e));
          }
        }
      }

      std::random_shuffle( cfg.default_peers.begin(), cfg.default_peers.end() );
      return cfg;
} FC_RETHROW_EXCEPTIONS( warn, "unable to load config file ${cfg}", ("cfg",datadir/"config.json")) }



  using namespace bts::wallet;
  using namespace bts::blockchain;

  // wrap the exception database in a class that logs the exception to the normal logging stream
  // in addition to just storing it
  class logging_exception_db
  {
  public:
    typedef bts::db::level_map<fc::time_point,fc::exception> exception_leveldb_type;
  private:
    exception_leveldb_type _db;
  public:
    void open(const fc::path& filename, bool create = true)
    {
      _db.open(filename, create);
    }
    void store(const fc::exception& e)
    {
      elog("storing error in database: ${e}", ("e", e));
      _db.store(fc::time_point::now(), e);
    }
    exception_leveldb_type::iterator lower_bound(const fc::time_point& time) const
    {
      return _db.lower_bound(time);
    }
    exception_leveldb_type::iterator begin() const
    {
      return _db.begin();
    }
    void remove(const fc::time_point& key)
    {
        _db.remove(key);
    }
  };


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
              user_appender( client_impl& c ) :
                _client_impl(c),
                _thread(&fc::thread::current())
              {}

                  virtual void log( const fc::log_message& m ) override
                  {
                if (!_thread->is_current())
                  return _thread->async([&](){ log(m); }, "user_appender::log").wait();

                     string format = m.get_format();
                     // lookup translation on format here

                     // perform variable substitution;
                     string message = format_string( format, m.get_data() );


                        _history.emplace_back( message );
                        if( _client_impl._cli )
                          _client_impl._cli->display_status_message( message );
                        else
                          std::cout << message << "\n";

                     // call a callback to the client...

                     // we need an RPC call to fetch this log and display the
                     // current status.
                  }

                  vector<string> get_history()const
                  {
                if (!_thread->is_current())
                  return _thread->async([&](){ return get_history(); }, "user_appender::get_history").wait();
                     return _history;
                  }

                  void clear_history()
                  {
                if (!_thread->is_current())
                  return _thread->async([&](){ return clear_history(); }, "user_appender::clear_history").wait();
                     _history.clear();
                  }

               private:
                  // TODO: consider a deque and enforce maximum length?
                  vector<string>        _history;
                  client_impl&          _client_impl;
              fc::thread*           _thread;
            };

            client_impl(bts::client::client* self, const std::string& user_agent) :
              _self(self),
              _user_agent(user_agent),
              _last_sync_status_message_indicated_in_sync(true),
              _last_sync_status_head_block(0),
              _remaining_items_to_sync(0),
              _sync_speed_accumulator(boost::accumulators::tag::rolling_window::window_size = 5),
              _connection_count_notification_interval(fc::minutes(5)),
              _connection_count_always_notify_threshold(5),
              _connection_count_last_value_displayed(0),
              _blockchain_synopsis_size_limit((unsigned)(log2(BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * 20)))
            {
            try
            {
              _user_appender = fc::shared_ptr<user_appender>( new user_appender(*this) );
              fc::logger::get( "user" ).add_appender( _user_appender );
              try
              {
                _rpc_server = std::make_shared<rpc_server>(self);
              } FC_RETHROW_EXCEPTIONS(warn,"rpc server")
              try
              {
                _chain_db = std::make_shared<chain_database>();
              } FC_RETHROW_EXCEPTIONS(warn,"chain_db")
            } FC_RETHROW_EXCEPTIONS( warn, "" ) }

            virtual ~client_impl() override
            {
              cancel_blocks_too_old_monitor_task();
              cancel_rebroadcast_pending_loop();
              _p2p_node.reset();
              delete _cli;
            }

            void start()
            {
              std::exception_ptr unexpected_exception;
              try
              {
                _cli->start();
              }
              catch (...)
              {
                unexpected_exception = std::current_exception();
              }

              if (unexpected_exception)
              {
                if (_notifier)
                  _notifier->notify_client_exiting_unexpectedly();
                std::rethrow_exception(unexpected_exception);
              }
            }

            void reschedule_delegate_loop();
            void start_delegate_loop();
            void cancel_delegate_loop();
            void delegate_loop();
            void set_target_connections( uint32_t target );

            void start_rebroadcast_pending_loop();
            void cancel_rebroadcast_pending_loop();
            void rebroadcast_pending_loop();
            fc::future<void> _rebroadcast_pending_loop_done;

            void configure_rpc_server(config& cfg,
                                      const program_options::variables_map& option_variables);
            void configure_chain_server(config& cfg,
                                        const program_options::variables_map& option_variables);

            block_fork_data on_new_block(const full_block& block,
                                         const block_id_type& block_id,
                                         bool sync_mode);

            bool on_new_transaction(const signed_transaction& trx);
            void blocks_too_old_monitor_task();
            void cancel_blocks_too_old_monitor_task();

            /* Implement node_delegate */
            // @{
            virtual bool has_item(const bts::net::item_id& id) override;
            virtual bool handle_message(const bts::net::message&, bool sync_mode) override;
            virtual std::vector<bts::net::item_hash_t> get_item_ids(uint32_t item_type,
                                                                    const vector<bts::net::item_hash_t>& blockchain_synopsis,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit = 2000) override;
            virtual bts::net::message get_item(const bts::net::item_id& id) override;
            virtual fc::sha256 get_chain_id() const override
            {
                FC_ASSERT( _chain_db != nullptr );
                return _chain_db->chain_id();
            }
            virtual std::vector<bts::net::item_hash_t> get_blockchain_synopsis(uint32_t item_type,
                                                                               const bts::net::item_hash_t& reference_point = bts::net::item_hash_t(),
                                                                               uint32_t number_of_blocks_after_reference_point = 0) override;
            virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
            virtual void connection_count_changed(uint32_t c) override;
            virtual uint32_t get_block_number(const bts::net::item_hash_t& block_id) override;
            virtual fc::time_point_sec get_block_time(const bts::net::item_hash_t& block_id) override;
            virtual fc::time_point_sec get_blockchain_now() override;
            virtual bts::net::item_hash_t get_head_block_id() const;
            virtual void error_encountered(const std::string& message, const fc::oexception& error) override;
            /// @}

            bts::client::client*                                    _self;
            std::string                                             _user_agent;
            bts::cli::cli*                                          _cli = nullptr;

#ifndef DISABLE_DELEGATE_NETWORK
            bts::network::node                                      _delegate_network;
#endif

            std::unique_ptr<std::istream>                           _command_script_holder;
            std::ofstream                                           _console_log;
            std::unique_ptr<std::ostream>                           _output_stream;
            std::unique_ptr<TeeDevice>                              _tee_device;
            std::unique_ptr<TeeStream>                              _tee_stream;
            bool                                                    _enable_ulog = false;

            fc::path                                                _data_dir;

            fc::shared_ptr<user_appender>                           _user_appender;
            bool                                                    _simulate_disconnect = false;
            fc::scoped_connection                                   _time_discontinuity_connection;

            bts::rpc::rpc_server_ptr                                _rpc_server = nullptr;
            std::unique_ptr<bts::net::chain_server>                 _chain_server = nullptr;
            std::unique_ptr<bts::net::upnp_service>                 _upnp_service = nullptr;
            chain_database_ptr                                      _chain_db = nullptr;
            unordered_map<transaction_id_type, signed_transaction>  _pending_trxs;
            wallet_ptr                                              _wallet = nullptr;
            std::shared_ptr<bts::mail::server>                      _mail_server = nullptr;
            std::shared_ptr<bts::mail::client>                      _mail_client = nullptr;
            fc::future<void>                                        _delegate_loop_complete;
            bool                                                    _delegate_loop_first_run = true;
            fc::time_point                                          _last_sync_status_message_time;
            bool                                                    _last_sync_status_message_indicated_in_sync;
            uint32_t                                                _last_sync_status_head_block;
            uint32_t                                                _remaining_items_to_sync;
            boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> > _sync_speed_accumulator;

            fc::time_point                                          _last_connection_count_message_time;
            /** display messages about the connection count changing at most once every _connection_count_notification_interval */
            fc::microseconds                                        _connection_count_notification_interval;
            /** exception: if you have fewer than _connection_count_always_notify_threshold connections, notify any time the count changes */
            uint32_t                                                _connection_count_always_notify_threshold;
            uint32_t                                                _connection_count_last_value_displayed;

            config                                                  _config;
            logging_exception_db                                    _exception_db;

            uint32_t                                                _min_delegate_connection_count = BTS_MIN_DELEGATE_CONNECTION_COUNT;
            //start by assuming not syncing, network won't send us a msg if we start synced and stay synched.
            //at worst this means we might briefly sending some pending transactions while not synched.
            bool                                                    _sync_mode = false;

            rpc_server_config                                       _tmp_rpc_config;
            bts::net::node_ptr                                      _p2p_node = nullptr;
            bts_gntp_notifier_ptr                                   _notifier;
            fc::future<void>                                        _blocks_too_old_monitor_done;

            const unsigned                                          _blockchain_synopsis_size_limit;

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
        if( option_variables.count("server") || option_variables.count("daemon") || cfg.rpc.enable )
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
          if (option_variables.count("httpdendpoint"))
            cfg.rpc.httpd_endpoint = fc::ip::endpoint::from_string(option_variables["httpdendpoint"].as<string>());
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
          bool rpc_success = _rpc_server->configure_rpc(cfg.rpc);
          rpc_success &= _rpc_server->configure_http(cfg.rpc);

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

      void client_impl::configure_chain_server(config& cfg, const program_options::variables_map& option_variables)
      {
        if( option_variables.count("chain-server-port") )
        {
          cfg.chain_server.listen_port = option_variables["chain-server-port"].as<uint16_t>();
          cfg.chain_server.enabled = true;
        }
      }

       // Call this whenever a change occurs that may enable block production by the client
       void client_impl::reschedule_delegate_loop()
       {
         if( !_delegate_loop_complete.valid() || _delegate_loop_complete.ready() )
           start_delegate_loop();
       }

       void client_impl::start_delegate_loop()
       {
          if (!_time_discontinuity_connection.connected())
            _time_discontinuity_connection = bts::blockchain::time_discontinuity_signal.connect([=](){ reschedule_delegate_loop(); });
          _delegate_loop_complete = fc::async( [=](){ delegate_loop(); }, "delegate_loop" );
       }

       void client_impl::cancel_delegate_loop()
       {
          try
          {
            ilog( "Canceling delegate loop..." );
            _delegate_loop_complete.cancel_and_wait(__FUNCTION__);
            ilog( "Delegate loop canceled" );
          }
          catch( const fc::exception& e )
          {
            wlog( "Unexpected exception thrown from delegate_loop(): ${e}", ("e",e.to_detail_string() ) );
          }
       }

       void client_impl::delegate_loop()
       {
          if( !_wallet->is_open() || _wallet->is_locked() )
              return;

          vector<wallet_account_record> enabled_delegates = _wallet->get_my_delegates( enabled_delegate_status );
          if( enabled_delegates.empty() )
              return;

          const auto now = blockchain::now();
          ilog( "Starting delegate loop at time: ${t}", ("t",now) );

          if( _delegate_loop_first_run )
          {
              set_target_connections( BTS_NET_DELEGATE_DESIRED_CONNECTIONS );
              _delegate_loop_first_run = false;
          }

          const auto next_block_time = _wallet->get_next_producible_block_timestamp( enabled_delegates );
          if( next_block_time.valid() )
          {
              // delegates don't get to skip this check, they must check up on everyone else
              _chain_db->skip_signature_verification( false );
              ilog( "Producing block at time: ${t}", ("t",*next_block_time) );

#ifndef DISABLE_DELEGATE_NETWORK
              // sign in to delegate server using private keys of my delegates
              //_delegate_network.signin( _wallet->get_my_delegate( enabled_delegate_status | active_delegate_status ) );
#endif

              if( *next_block_time <= now )
              {
                  try
                  {
                      FC_ASSERT( network_get_connection_count() >= _min_delegate_connection_count,
                                 "Client must have ${count} connections before you may produce blocks!",
                                 ("count",_min_delegate_connection_count) );
                      FC_ASSERT( _wallet->is_unlocked(), "Wallet must be unlocked to produce blocks!" );
                      FC_ASSERT( (now - *next_block_time) < fc::seconds( BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC ),
                                 "You missed your slot at time: ${t}!", ("t",*next_block_time) );

                      full_block next_block = _chain_db->generate_block( *next_block_time );
                      _wallet->sign_block( next_block );
                      on_new_block( next_block, next_block.id(), false );

#ifndef DISABLE_DELEGATE_NETWORK
                      _delegate_network.broadcast_block( next_block );
                      // broadcast block to delegates first, starting with the next delegate
#endif

                      _p2p_node->broadcast( block_message( next_block ) );
                      ilog( "Produced block #${n}!", ("n",next_block.block_num) );
                   }
                   catch ( const fc::canceled_exception& )
                   {
                      throw;
                   }
                   catch( const fc::exception& e )
                   {
                      _exception_db.store( e );
                   }
              }
          }

          uint32_t slot_number = blockchain::get_slot_number( now );
          time_point_sec next_slot_time = blockchain::get_slot_start_time( slot_number + 1 );
          ilog( "Rescheduling delegate loop for time: ${t}", ("t",next_slot_time) );

          time_point scheduled_time = next_slot_time;
          if( blockchain::ntp_time().valid() )
            scheduled_time -= blockchain::ntp_error();

          /* Don't reschedule immediately in case we are in simulation */
          const auto system_now = time_point::now();
          if( scheduled_time <= system_now )
            scheduled_time = system_now + fc::seconds( 1 );

          if (!_delegate_loop_complete.canceled())
            _delegate_loop_complete = fc::schedule( [=](){ delegate_loop(); }, scheduled_time, "delegate_loop" );
       }

       void client_impl::set_target_connections( uint32_t target )
       {
           auto params = fc::mutable_variant_object();
           params["desired_number_of_connections"] = target;
           network_set_advanced_node_parameters( params );
       }

       vector<account_record> client_impl::blockchain_list_active_delegates( uint32_t first, uint32_t count )const
       {
          if( first > 0 ) --first;
          FC_ASSERT( first < BTS_BLOCKCHAIN_NUM_DELEGATES );
          FC_ASSERT( first + count <= BTS_BLOCKCHAIN_NUM_DELEGATES );
          vector<account_id_type> all_delegate_ids = _chain_db->get_active_delegates();
          FC_ASSERT( all_delegate_ids.size() == BTS_BLOCKCHAIN_NUM_DELEGATES );
          vector<account_id_type> delegate_ids( all_delegate_ids.begin() + first, all_delegate_ids.begin() + first + count );

          vector<account_record> delegate_records;
          delegate_records.reserve( count );
          for( const auto& delegate_id : delegate_ids )
          {
            auto delegate_record = _chain_db->get_account_record( delegate_id );
            FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
            delegate_records.push_back( *delegate_record );
          }
          return delegate_records;
       }

       vector<account_record> client_impl::blockchain_list_delegates( uint32_t first, uint32_t count )const
       {
          if( first > 0 ) --first;
          vector<account_id_type> delegate_ids = _chain_db->get_delegates_by_vote( first, count );

          vector<account_record> delegate_records;
          delegate_records.reserve( count );
          for( const auto& delegate_id : delegate_ids )
          {
            auto delegate_record = _chain_db->get_account_record( delegate_id );
            FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
            delegate_records.push_back( *delegate_record );
          }
          return delegate_records;
       }

       vector<string> client_impl::blockchain_list_missing_block_delegates( uint32_t block_num )
       {
           if (block_num == 0 || block_num == 1)
               return vector<string>();
           vector<string> delegates;
           auto this_block = _chain_db->get_block_record( block_num );
           FC_ASSERT(this_block.valid(), "Cannot use this call on a block that has not yet been produced");
           auto prev_block = _chain_db->get_block_record( block_num - 1 );
           auto timestamp = prev_block->timestamp;
           timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
           while (timestamp != this_block->timestamp)
           {
               auto slot_record = _chain_db->get_slot_record( timestamp );
               FC_ASSERT( slot_record.valid() );
               auto delegate_record = _chain_db->get_account_record( slot_record->block_producer_id );
               FC_ASSERT( delegate_record.valid() );
               delegates.push_back( delegate_record->name );
               timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
           }
           return delegates;
       }

       vector<block_record> client_impl::blockchain_list_blocks( uint32_t first, int32_t count )
       {
          FC_ASSERT( count <= 1000 );
          FC_ASSERT( count >= -1000 );
          vector<block_record> result;
          if (count == 0) return result;

          uint32_t total_blocks = _chain_db->get_head_block_num();
          FC_ASSERT( first <= total_blocks );

          int32_t increment = 1;

          //Normalize first and count if count < 0 and adjust count if necessary to not try to list nonexistent blocks
          if( count < 0 )
          {
            first = total_blocks - first;
            count *= -1;
            if( signed(first) - count < 0 )
              count = first;
            increment = -1;
          }
          else
          {
            if ( first == 0 )
              first = 1;
            if( first + count - 1 > total_blocks )
              count = total_blocks - first + 1;
          }
          result.reserve( count );

          for( int32_t block_num = first; count; --count, block_num += increment )
          {
            auto record = _chain_db->get_block_record( block_num );
            FC_ASSERT( record.valid() );
            result.push_back( *record );
          }

          return result;
       }

       signed_transactions client_impl::blockchain_list_pending_transactions() const
       {
         signed_transactions trxs;
         vector<transaction_evaluation_state_ptr> pending = _chain_db->get_pending_transactions();
         trxs.reserve(pending.size());
         for (auto trx_eval_ptr : pending)
         {
           trxs.push_back(trx_eval_ptr->trx);
         }
         return trxs;
       }

       void client_impl::start_rebroadcast_pending_loop()
       {
         if (!_rebroadcast_pending_loop_done.valid() || _rebroadcast_pending_loop_done.ready())
           _rebroadcast_pending_loop_done = fc::schedule( [=](){ rebroadcast_pending_loop(); },
                                                          fc::time_point::now() + fc::seconds((int64_t)(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 1.3)),
                                                          "rebroadcast_pending" );
       }

       void client_impl::cancel_rebroadcast_pending_loop()
       {
          try
          {
            _rebroadcast_pending_loop_done.cancel_and_wait(__FUNCTION__);
          }
          catch (const fc::exception& e)
          {
            wlog("Unexpected error from rebroadcast_pending(): ${e}", ("e", e));
          }
       }

       void client_impl::rebroadcast_pending_loop()
       {
#ifndef NDEBUG
          static bool currently_running = false;
          struct checker {
            bool& var;
            //Log instead of crashing. Failing this test is normal behavior in the tests.
            checker(bool& var) : var(var) { if(var) elog("Checker failure!"); var = true; }
            ~checker() { var = false; }
          } _checker(currently_running);
#endif // !NDEBUG
          if (_sync_mode)
          {
            wlog("skip rebroadcast_pending while syncing");
          }
          else
          {
            wlog( " rebroadcasting..." );
            try
            {
              signed_transactions pending = blockchain_list_pending_transactions();
              for( auto trx : pending )
              {
                network_broadcast_transaction( trx );
              }
            }
            catch ( const fc::canceled_exception& )
            {
              throw;
            }
            catch ( const fc::exception& e )
            {
              wlog( "error rebroadcasting transacation: ${e}", ("e",e.to_detail_string() ) );
            }
          }
          if (!_rebroadcast_pending_loop_done.canceled())
            _rebroadcast_pending_loop_done = fc::schedule( [=](){ rebroadcast_pending_loop(); },
                                                           fc::time_point::now() + fc::seconds((int64_t)(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 1.3)),
                                                           "rebroadcast_pending" );
       }

       ///////////////////////////////////////////////////////
       // Implement chain_client_delegate                   //
       ///////////////////////////////////////////////////////
       block_fork_data client_impl::on_new_block(const full_block& block,
                                                 const block_id_type& block_id,
                                                 bool sync_mode)
       {
         try
         {
            _sync_mode = sync_mode;
            if (sync_mode && _remaining_items_to_sync > 0)
              --_remaining_items_to_sync;
            try
            {
              FC_ASSERT( !_simulate_disconnect );
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

                   fc::time_point_sec now = blockchain::now();
                   fc::time_point_sec head_block_timestamp = _chain_db->now();
                   if (_cli
                       && result.is_included
                       && (now - head_block_timestamp) > fc::minutes(5)
                       && _last_sync_status_message_time < (now - fc::seconds(30)))
                   {
                      std::ostringstream message;
                      message << "--- syncing with p2p network, our last block is "
                              << fc::get_approximate_relative_time_string(head_block_timestamp, now, " old");
                      ulog( message.str() );
                      uint32_t current_head_block_num = _chain_db->get_head_block_num();
                      if (_last_sync_status_message_time > (now - fc::seconds(60)) &&
                          _last_sync_status_head_block != 0 &&
                          current_head_block_num > _last_sync_status_head_block)
                      {
                        uint32_t seconds_since_last_status_message = (uint32_t)((fc::time_point(now) - _last_sync_status_message_time).count() / fc::seconds(1).count());
                        uint32_t blocks_since_last_status_message = current_head_block_num - _last_sync_status_head_block;
                        double current_sync_speed_in_blocks_per_sec = (double)blocks_since_last_status_message / seconds_since_last_status_message;
                        _sync_speed_accumulator(current_sync_speed_in_blocks_per_sec);
                        double average_sync_speed = boost::accumulators::rolling_mean(_sync_speed_accumulator);
                        std::ostringstream speed_message;
                        if (average_sync_speed > 0)
                        {
                          double remaining_seconds_to_sync = _remaining_items_to_sync / average_sync_speed;

                          speed_message << "--- currently syncing at ";
                          if (average_sync_speed >= 10.)
                            speed_message << (int)average_sync_speed << " blocks/sec, ";
                          else if (average_sync_speed >= 0.1)
                            speed_message << std::setprecision(2) << average_sync_speed << " blocks/sec, ";
                          else
                            speed_message << (int)(1./average_sync_speed) << " sec/block, ";
                          speed_message << fc::get_approximate_relative_time_string(fc::time_point::now(), fc::time_point::now() + fc::seconds((int64_t)remaining_seconds_to_sync), "") << " remaining";
                        }
                        else
                          speed_message << "--- currently syncing at an imperceptible rate";
                        ulog(speed_message.str());
                      }
                      _last_sync_status_message_time = now;
                      _last_sync_status_head_block = current_head_block_num;
                      _last_sync_status_message_indicated_in_sync = false;
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
            _exception_db.store(e);
            throw;
         }
       }

       bool client_impl::on_new_transaction(const signed_transaction& trx)
       {
          try {
              // throws exception if invalid trx, don't override limits
              return !!_chain_db->store_pending_transaction(trx, false);
          }
          catch ( const duplicate_transaction& )
          {
             throw;
          }
          catch ( const fc::exception& e )
          {
             _exception_db.store(e);
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
        try
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
                return on_new_transaction(trx_message_to_handle.trx);
            }
          }
          return false;
        }
        catch (const bts::blockchain::insufficient_relay_fee& original_exception)
        {
          FC_THROW_EXCEPTION(bts::net::insufficient_relay_fee, "Insufficient relay fee; do not propagate!",
                             ("original_exception", original_exception.to_detail_string()));
        }
        catch (const bts::blockchain::block_older_than_undo_history& original_exception)
        {
          FC_THROW_EXCEPTION(bts::net::block_older_than_undo_history, "Block is older than undo history, stop fetching blocks!",
                             ("original_exception", original_exception.to_detail_string()));
        }
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

        // assume anything longer than our limit is an attacker (limit is currently ~26 items)
        if (blockchain_synopsis.size() > _blockchain_synopsis_size_limit)
          FC_THROW("Peer provided unreasonably long blockchain synopsis during sync (actual length: ${size}, limit: ${blockchain_synopsis_size_limit})",
                   ("size", blockchain_synopsis.size())
                   ("blockchain_synopsis_size_limit", _blockchain_synopsis_size_limit));

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
          block_id_type block_id;
          bool block_id_not_found = false;
          try
          {
            block_id = _chain_db->get_block_id(last_seen_block_num);
            //assert(_chain_db->get_block(last_seen_block_num).id() == block_id);  // expensive assert, remove once we're sure
          }
          catch (const fc::key_not_found_exception&)
          {
            block_id_not_found = true;
          }

          if (block_id_not_found)
          {
            ilog("chain_database::get_block_id failed to return the id for block number ${last_seen_block_num} even though chain_database::get_block_num() provided its block number",
                 ("last_seen_block_num",last_seen_block_num));
            ulog("Error: your chain database is in an inconsistent state.  Please shut down and relaunch using --rebuild-index or --resync-blockchain to repair the database");
            assert(!"I assume this can never happen");
            // our database doesn't make sense, so just act as if we have no blocks so the remote node doesn't try to sync with us
            remaining_item_count = 0;
            hashes_to_return.clear();
            return hashes_to_return;
          }
          hashes_to_return.push_back(block_id);
          ++last_seen_block_num;
        }
        remaining_item_count -= items_to_get_this_iteration;
        return hashes_to_return;
      }

      std::vector<bts::net::item_hash_t> client_impl::get_blockchain_synopsis(uint32_t item_type,
                                                                              const bts::net::item_hash_t& reference_point /* = bts::net::item_hash_t() */,
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
                  if( last_non_fork_block == block_id_type() )
                    non_fork_high_block_num = 0;
                  else
                    non_fork_high_block_num = _chain_db->get_block_num(last_non_fork_block);
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
                throw; //FC_RETHROW_EXCEPTIONS( e ); //throw;
                //return synopsis;
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
          // for each block in the synopsis, figure out where to pull the block id from.
          // if it's <= non_fork_high_block_num, we grab it from the main blockchain;
          // if it's not, we pull it from the fork history
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

       void client_impl::execute_script(const fc::path& script_filename) const
       {
         if (_cli)
         {
           if (!fc::exists(script_filename))
             FC_THROW_EXCEPTION(fc::file_not_found_exception, "Script file not found!");
           string input_commands = extract_commands_from_log_file(script_filename);
           std::stringstream input_stream(input_commands);
           _cli->process_commands( &input_stream );
           _cli->process_commands( &std::cin );
         }
       }

       fc::variants client_impl::batch( const std::string& method_name,
                                        const std::vector<fc::variants>& parameters_list) const
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
         const bool in_sync = item_count == 0;
         _remaining_items_to_sync = item_count;

         fc::time_point now = fc::time_point::now();
         if (_cli)
         {
           if (in_sync && !_last_sync_status_message_indicated_in_sync)
           {
             ulog( "--- in sync with p2p network" );
             _last_sync_status_message_time = now;
             _last_sync_status_message_indicated_in_sync = true;
             _last_sync_status_head_block = 0;
           }
           else if (!in_sync &&
                    item_count >= 100 && // if we're only a few blocks out of sync, don't bother the user about it
                    _last_sync_status_message_indicated_in_sync &&
                    _last_sync_status_message_time < now - fc::seconds(30))
           {
             std::ostringstream message;
             message << "--- syncing with p2p network, " << item_count << " blocks left to fetch";
             ulog( message.str() );
             _last_sync_status_message_time = now;
             _last_sync_status_message_indicated_in_sync = false;
             _last_sync_status_head_block = _chain_db->get_head_block_num();
           }
         }
       }

       void client_impl::connection_count_changed(uint32_t new_connection_count)
       {
         fc::time_point now(fc::time_point::now());
         if (new_connection_count != _connection_count_last_value_displayed &&
             (new_connection_count < _connection_count_always_notify_threshold ||
              now > _last_connection_count_message_time + _connection_count_notification_interval))
         {
           _last_connection_count_message_time = now;
           _connection_count_last_value_displayed = new_connection_count;
           std::ostringstream message;
           message << "--- there are now " << new_connection_count << " active connections to the p2p network";
           ulog( message.str() );
         }
         if (_notifier)
           _notifier->notify_connection_count_changed(new_connection_count);
       }

       uint32_t client_impl::get_block_number(const bts::net::item_hash_t& block_id)
       {
         return _chain_db->get_block_num(block_id);
       }

       fc::time_point_sec client_impl::get_block_time(const bts::net::item_hash_t& block_id)
       {
         if (block_id == bts::net::item_hash_t())
         {
           // then the question the net is really asking is, what is the timestamp of the
           // genesis block?  That's not stored off directly anywhere I can find, but it
           // does wind its way into the the registration date of the base asset.
           oasset_record base_asset_record = _chain_db->get_asset_record(BTS_BLOCKCHAIN_SYMBOL);
           FC_ASSERT(base_asset_record);
           if (!base_asset_record)
             return fc::time_point_sec::min();
           return base_asset_record->registration_date;
         }
         // else they're asking about a specific block
         try
         {
           return _chain_db->get_block_header(block_id).timestamp;
         }
         catch ( const fc::canceled_exception& )
         {
           throw;
         }
         catch (const fc::exception&)
         {
           return fc::time_point_sec::min();
         }
       }

       fc::time_point_sec client_impl::get_blockchain_now()
       {
         return bts::blockchain::now();
       }

       bts::net::item_hash_t client_impl::get_head_block_id() const
       {
         return _chain_db->get_head_block_id();
       }

      void client_impl::error_encountered(const std::string& message, const fc::oexception& error)
      {
        if (error)
          _exception_db.store(*error);
        else
          _exception_db.store(fc::exception(FC_LOG_MESSAGE(error, message.c_str())));
        ulog( message );
      }

      void client_impl::blocks_too_old_monitor_task()
      {
        // if we have no connections, don't warn about the head block too old --
        //   we should already be warning about no connections
        // if we're syncing, don't warn, we wouldn't be syncing if the head block weren't old
        if (_chain_db->get_head_block().timestamp < bts::blockchain::now() - fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 2) &&
            !_sync_mode &&
            _p2p_node->get_connection_count() > 0 &&
            _notifier)
          _notifier->notify_head_block_too_old(_chain_db->get_head_block().timestamp);

        if (!_blocks_too_old_monitor_done.canceled())
          _blocks_too_old_monitor_done = fc::schedule([=]() { blocks_too_old_monitor_task(); },
                                                      fc::time_point::now() + fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC),
                                                      "block_monitor_task");
      }

      void client_impl::cancel_blocks_too_old_monitor_task()
      {
        try
        {
          _blocks_too_old_monitor_done.cancel_and_wait(__FUNCTION__);
        }
        catch( const fc::exception& e )
        {
          wlog( "Unexpected exception thrown while canceling blocks_too_old_monitor(): ${e}", ("e",e.to_detail_string() ) );
        }
      }

    } // end namespace detail

    client::client(const std::string& user_agent)
    :my(new detail::client_impl(this, user_agent))
    {
    }

    client::client(const std::string& user_agent,
                   bts::net::simulated_network_ptr network_to_connect_to)
    : my( new detail::client_impl(this, user_agent) )
    {
      network_to_connect_to->add_node_delegate(my.get());
      my->_p2p_node = network_to_connect_to;
    }

    void client::simulate_disconnect( bool state )
    {
       my->_simulate_disconnect = state;
    }

    void client::open( const path& data_dir, fc::optional<fc::path> genesis_file_path, std::function<void(float)> reindex_status_callback )
    { try {
        my->_config = load_config( data_dir, my->_enable_ulog );

#ifndef DISABLE_DELEGATE_NETWORK
        /*
         *  Don't delete me, I promise I will be used soon
         *
         *  TODO: this creates a memory leak / circular reference between client and
         *  delegate network.
        */
        my->_delegate_network.set_client( shared_from_this() );
        my->_delegate_network.listen( my->_config.delegate_server );

        for( auto delegate_host : my->_config.default_delegate_peers )
        {
           try {
             wlog( "connecting to delegate peer ${p}", ("p",delegate_host) );
             my->_delegate_network.connect_to( fc::ip::endpoint::from_string(delegate_host) );
           }
           catch ( const fc::exception& e )
           {
              wlog( "${e}", ("e", e.to_detail_string() ) );
           }

        }
#endif

        //std::cout << fc::json::to_pretty_string( cfg ) <<"\n";
        fc::configure_logging( my->_config.logging );
        // re-register the _user_appender which was overwritten by configure_logging()
        fc::logger::get( "user" ).add_appender( my->_user_appender );

        try
        {
          my->_exception_db.open( data_dir / "exceptions", true );
        }
        catch( const db::db_in_use_exception& e )
        {
          if( e.to_string().find("Corruption") != string::npos )
          {
            elog("Exception database corrupted. Deleting it and attempting to recover.");
            fc::remove_all( data_dir / "exceptions" );
            my->_exception_db.open( data_dir / "exceptions", true );
          }
          //FIXME: is it really correct to continue here without rethrowing?
        }

        bool attempt_to_recover_database = false;
        try
        {
          my->_chain_db->open( data_dir / "chain", genesis_file_path, reindex_status_callback );
        }
        catch( const db::db_in_use_exception& e )
        {
          if (e.to_string().find("Corruption") != string::npos)
          {
            elog("Chain database corrupted. Deleting it and attempting to recover.");
            attempt_to_recover_database = true;
          }
          //FIXME: is it really correct to continue here without rethrowing?
        }
        catch ( const wrong_chain_id& )
        {
          elog("Wrong chain ID. Deleting database and attempting to recover.");
          attempt_to_recover_database = true;
        }

        if (attempt_to_recover_database)
        {
          fc::remove_all(data_dir / "chain");
          my->_chain_db->open(data_dir / "chain", genesis_file_path, reindex_status_callback);
        }

        my->_wallet = std::make_shared<bts::wallet::wallet>( my->_chain_db, my->_config.wallet_enabled );
        my->_wallet->set_data_directory( data_dir / "wallets" );

        if (my->_config.mail_server_enabled)
        {
          my->_mail_server = std::make_shared<bts::mail::server>();
          my->_mail_server->open( data_dir / "mail" );
        }
        my->_mail_client = std::make_shared<bts::mail::client>(my->_wallet, my->_chain_db);
        my->_mail_client->open( data_dir / "mail_client" );

        //if we are using a simulated network, _p2p_node will already be set by client's constructor
        if (!my->_p2p_node)
          my->_p2p_node = std::make_shared<bts::net::node>(my->_user_agent);
        my->_p2p_node->set_node_delegate(my.get());

        my->start_rebroadcast_pending_loop();
    } FC_RETHROW_EXCEPTIONS( warn, "", ("data_dir",data_dir) ) }

    client::~client()
    {
      if (my->_notifier)
        my->_notifier->client_is_shutting_down();
      my->cancel_delegate_loop();
    }

    wallet_ptr client::get_wallet()const { return my->_wallet; }
    mail_client_ptr client::get_mail_client()const { return my->_mail_client; }
    mail_server_ptr client::get_mail_server()const { return my->_mail_server; }
    chain_database_ptr client::get_chain()const { return my->_chain_db; }
    bts::rpc::rpc_server_ptr client::get_rpc_server()const { return my->_rpc_server; }
    bts::net::node_ptr client::get_node()const { return my->_p2p_node; }

    fc::variant_object version_info()
    {
      string client_version( bts::utilities::git_revision_description );
#ifdef BTS_TEST_NETWORK
      client_version += "-testnet-" + std::to_string( BTS_TEST_NETWORK_VERSION );
#endif

      fc::mutable_variant_object info;
      info["blockchain_name"]                   = BTS_BLOCKCHAIN_NAME;
      info["blockchain_description"]            = BTS_BLOCKCHAIN_DESCRIPTION;
      info["client_version"]                    = client_version;
      info["bitshares_toolkit_revision"]        = bts::utilities::git_revision_sha;
      info["bitshares_toolkit_revision_age"]    = fc::get_approximate_relative_time_string( fc::time_point_sec( bts::utilities::git_revision_unix_timestamp ) );
      info["fc_revision"]                       = fc::git_revision_sha;
      info["fc_revision_age"]                   = fc::get_approximate_relative_time_string( fc::time_point_sec( fc::git_revision_unix_timestamp ) );
      info["compile_date"]                      = "compiled on " __DATE__ " at " __TIME__;
      info["boost_version"]                     = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
      info["openssl_version"]                   = OPENSSL_VERSION_TEXT;

      std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
      std::string os = "osx";
#elif defined(__linux__)
      std::string os = "linux";
#elif defined(_MSC_VER)
      std::string os = "win32";
#else
      std::string os = "other";
#endif
      info["build"] = os + " " + bitness;

#if defined(BTS_CLIENT_JENKINS_BUILD_NUMBER)
      info["jenkins_build_number"] = BTS_CLIENT_JENKINS_BUILD_NUMBER;
#endif
#if defined(BTS_CLIENT_JENKINS_BUILD_URL)
      info["jenkins_build_url"] = BTS_CLIENT_JENKINS_BUILD_URL;
#endif

      return info;
    }

    bts::blockchain::transaction_id_type detail::client_impl::network_broadcast_transaction(const bts::blockchain::signed_transaction& transaction_to_broadcast)
    {
      // ilog("broadcasting transaction: ${id} ", ("id", transaction_to_broadcast.id()));

      // p2p doesn't send messages back to the originator
      _p2p_node->broadcast(trx_message(transaction_to_broadcast));
      on_new_transaction(transaction_to_broadcast);
      return transaction_to_broadcast.id();
    }

    //JSON-RPC Method Implementations START
    block_id_type detail::client_impl::blockchain_get_block_hash( uint32_t block_number ) const
    {
      return _chain_db->get_block(block_number).id();
    }

    uint32_t detail::client_impl::blockchain_get_block_count() const
    {
      return _chain_db->get_head_block_num();
    }

    void detail::client_impl::wallet_open(const string& wallet_name)
    {
      _wallet->open(fc::trim(wallet_name));
      reschedule_delegate_loop();
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
      reschedule_delegate_loop();
    }

    fc::optional<string> detail::client_impl::wallet_get_name() const
    {
      return _wallet->is_open() ? _wallet->get_wallet_name() : fc::optional<string>();
    }

    void detail::client_impl::wallet_close()
    {
        if (_wallet) {
            _wallet->close();
            reschedule_delegate_loop();
        }
    }

    void detail::client_impl::wallet_backup_create( const fc::path& json_filename )const
    {
        _wallet->export_to_json( json_filename );
    }

    void detail::client_impl::wallet_backup_restore( const fc::path& json_filename, const string& wallet_name, const string& imported_wallet_passphrase )
    {
        _wallet->create_from_json( json_filename, wallet_name, imported_wallet_passphrase );
        reschedule_delegate_loop();
    }

    bool detail::client_impl::wallet_set_automatic_backups( bool enabled )
    {
        _wallet->set_automatic_backups( enabled );
        return _wallet->get_automatic_backups();
    }

    uint32_t detail::client_impl::wallet_set_transaction_expiration_time( uint32_t secs )
    {
        _wallet->set_transaction_expiration( secs );
        return _wallet->get_transaction_expiration();
    }

    void detail::client_impl::wallet_lock()
    {
      _wallet->lock();
      reschedule_delegate_loop();
    }

    void detail::client_impl::wallet_unlock( uint32_t timeout, const string& password )
    {
      _wallet->unlock(password, timeout);
      reschedule_delegate_loop();
    }

    void detail::client_impl::wallet_change_passphrase(const string& new_password)
    {
      _wallet->auto_backup( "passphrase_change" );
      _wallet->change_passphrase(new_password);
      reschedule_delegate_loop();
    }

    map<transaction_id_type, fc::exception> detail::client_impl::wallet_get_pending_transaction_errors( const string& filename )const
    {
      const auto& errors = _wallet->get_pending_transaction_errors();
      if( filename != "" )
      {
          FC_ASSERT( !fc::exists( filename ) );
          std::ofstream out( filename.c_str() );
          out << fc::json::to_pretty_string( errors );
      }
      return errors;
    }

    wallet_transaction_record detail::client_impl::wallet_publish_slate( const string& publishing_account_name,
                                                                         const string& paying_account_name )
    {
       const auto record = _wallet->publish_slate( publishing_account_name, paying_account_name );
       network_broadcast_transaction( record.trx );
       return record;
    }

    wallet_transaction_record detail::client_impl::wallet_publish_version( const string& publishing_account_name,
                                                                           const string& paying_account_name )
    {
       const auto record = _wallet->publish_version( publishing_account_name, paying_account_name );
       network_broadcast_transaction( record.trx );
       return record;
    }

    int32_t detail::client_impl::wallet_recover_accounts( int32_t accounts_to_recover, int32_t maximum_number_of_attempts )
    {
      return _wallet->recover_accounts(accounts_to_recover, maximum_number_of_attempts);
    }

    wallet_transaction_record detail::client_impl::wallet_recover_transaction(
            const string& transaction_id_prefix, const string& recipient_account )
    {
        return _wallet->recover_transaction( transaction_id_prefix, recipient_account );
    }

    wallet_transaction_record detail::client_impl::wallet_edit_transaction(
            const string& transaction_id_prefix, const string& recipient_account, const string& memo_message )
    {
        return _wallet->edit_transaction( transaction_id_prefix, recipient_account, memo_message );
    }

    wallet_transaction_record detail::client_impl::wallet_transfer(
            double amount_to_transfer,
            const string& asset_symbol,
            const string& from_account_name,
            const string& to_account_name,
            const string& memo_message,
            const vote_selection_method& selection_method )
    {
        const auto record = _wallet->transfer_asset( amount_to_transfer, asset_symbol,
                                                     from_account_name, from_account_name, to_account_name,
                                                     memo_message, selection_method );
        network_broadcast_transaction( record.trx );
        return record;
    }
    wallet_transaction_record detail::client_impl::wallet_burn(
            double amount_to_transfer,
            const string& asset_symbol,
            const string& from_account_name,
            const string& for_or_against,
            const string& to_account_name,
            const string& public_message,
            bool anonymous )
    {
        const auto record = _wallet->burn_asset( amount_to_transfer, asset_symbol,
                                                     from_account_name, for_or_against, to_account_name,
                                                     public_message, anonymous );
        network_broadcast_transaction( record.trx );
        return record;
    }

    wallet_transaction_record detail::client_impl::wallet_transfer_from(
            double amount_to_transfer,
            const string& asset_symbol,
            const string& paying_account_name,
            const string& from_account_name,
            const string& to_account_name,
            const string& memo_message,
            const vote_selection_method& selection_method )
    {
        const auto record = _wallet->transfer_asset( amount_to_transfer, asset_symbol,
                                                     paying_account_name, from_account_name, to_account_name,
                                                     memo_message, selection_method );
        network_broadcast_transaction( record.trx );
        return record;
    }

    wallet_transaction_record detail::client_impl::wallet_asset_create(
            const string& symbol,
            const string& asset_name,
            const string& issuer_name,
            const string& description,
            const variant& data,
            double maximum_share_supply ,
            int64_t precision,
            bool is_market_issued /* = false */ )
    {
      const auto record = _wallet->create_asset( symbol, asset_name, description, data, issuer_name,
                                                 maximum_share_supply, precision, is_market_issued );
      network_broadcast_transaction( record.trx );
      return record;
    }

    wallet_transaction_record detail::client_impl::wallet_asset_issue(
            double real_amount,
            const string& symbol,
            const string& to_account_name,
            const string& memo_message )
    {
      const auto record = _wallet->issue_asset( real_amount, symbol, to_account_name, memo_message );
      network_broadcast_transaction( record.trx );
      return record;
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

    vector<wallet_account_record> detail::client_impl::wallet_list_favorite_accounts() const
    {
      return _wallet->list_favorite_accounts();
    }

    vector<wallet_account_record> detail::client_impl::wallet_list_unregistered_accounts() const
    {
      return _wallet->list_unregistered_accounts();
    }

    void detail::client_impl::wallet_remove_contact_account(const string& account_name)
    {
      _wallet->remove_contact_account( account_name );
    }

    void detail::client_impl::wallet_account_rename(const string& current_account_name,
                                       const string& new_account_name)
    {
      _wallet->rename_account(current_account_name, new_account_name);
      _wallet->auto_backup( "account_rename" );
    }

    wallet_account_record detail::client_impl::wallet_get_account(const string& account_name) const
    { try {
      return _wallet->get_account( account_name );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

    vector<pretty_transaction> detail::client_impl::wallet_account_transaction_history( const string& account_name,
                                                                                        const string& asset_symbol,
                                                                                        int32_t limit,
                                                                                        uint32_t start_block_num,
                                                                                        uint32_t end_block_num )const
    { try {
      const auto history = _wallet->get_pretty_transaction_history( account_name, start_block_num, end_block_num, asset_symbol );
      if( limit == 0 || abs( limit ) >= history.size() )
      {
          return history;
      }
      else if( limit > 0 )
      {
          return vector<pretty_transaction>( history.begin(), history.begin() + limit );
      }
      else
      {
          return vector<pretty_transaction>( history.end() - abs( limit ), history.end() );
      }
    } FC_RETHROW_EXCEPTIONS( warn, "") }

    void detail::client_impl::wallet_remove_transaction( const string& transaction_id )
    { try {
       _wallet->remove_transaction_record( transaction_id );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

    void detail::client_impl::wallet_rebroadcast_transaction( const string& transaction_id )
    { try {
       const auto records = _wallet->get_transactions( transaction_id );
       for( const auto& record : records )
       {
           if( record.is_virtual ) continue;
           network_broadcast_transaction( record.trx );
           std::cout << "Rebroadcasted transaction: " << string( record.trx.id() ) << "\n";
       }
    } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

    oaccount_record detail::client_impl::blockchain_get_account( const string& account )const
    {
      try
      {
          ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
          if( std::all_of( account.begin(), account.end(), ::isdigit) )
              return _chain_db->get_account_record( std::stoi( account ) );
          else if( account.substr( 0, string( BTS_ADDRESS_PREFIX ).size() ) == BTS_ADDRESS_PREFIX )
              return _chain_db->get_account_record( address( public_key_type( account ) ) );
          else
              return _chain_db->get_account_record( account );
      }
      catch( ... )
      {
      }
      return oaccount_record();
    }

    balance_record detail::client_impl::blockchain_get_balance( const balance_id_type& balance_id )const
    {
        const auto balance_record = _chain_db->get_balance_record( balance_id );
        FC_ASSERT( balance_record.valid() );
        return *balance_record;
    }

    oasset_record detail::client_impl::blockchain_get_asset( const string& asset )const
    {
      try
      {
          ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
          if( !std::all_of( asset.begin(), asset.end(), ::isdigit ) )
              return _chain_db->get_asset_record( asset );
          else
              return _chain_db->get_asset_record( std::stoi( asset ) );
      }
      catch( ... )
      {
      }
      return oasset_record();
    }

    //TODO: Refactor: most of these next two functions are identical. Should extract a function.
    vector<feed_entry> detail::client_impl::blockchain_get_feeds_for_asset(const std::string &asset) const
    { try {
        asset_id_type asset_id;
        if( !std::all_of( asset.begin(), asset.end(), ::isdigit) )
            asset_id = _chain_db->get_asset_id(asset);
        else
            asset_id = std::stoi( asset );

        auto raw_feeds = _chain_db->get_feeds_for_asset(asset_id);
        vector<feed_entry> result_feeds;
        for( auto feed : raw_feeds )
        {
            auto delegate = _chain_db->get_account_record(feed.feed.delegate_id);
            if( !delegate )
              FC_THROW_EXCEPTION( unknown_account, "Unknown delegate", ("delegate_id", feed.feed.delegate_id) );
            double price = _chain_db->to_pretty_price_double(feed.value.as<blockchain::price>());

            result_feeds.push_back({delegate->name, price, feed.last_update});
        }

        auto omedian_price = _chain_db->get_median_delegate_price(asset_id);
        if( omedian_price )
          result_feeds.push_back({"MARKET", 0, _chain_db->now(), _chain_db->get_asset_symbol(asset_id), _chain_db->to_pretty_price_double(*omedian_price)});

        return result_feeds;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("asset",asset) ) }

    vector<feed_entry> detail::client_impl::blockchain_get_feeds_from_delegate( const string& delegate_name )const
    { try {
        const auto delegate_record = _chain_db->get_account_record( delegate_name );
        if( !delegate_record.valid() || !delegate_record->is_delegate() )
            FC_THROW_EXCEPTION( unknown_account, "Unknown delegate account!" );

        const auto raw_feeds = _chain_db->get_feeds_from_delegate( delegate_record->id );
        vector<feed_entry> result_feeds;
        result_feeds.reserve( raw_feeds.size() );

        for( const auto& raw_feed : raw_feeds )
        {
            const double price = _chain_db->to_pretty_price_double( raw_feed.value.as<blockchain::price>() );
            const string asset_symbol = _chain_db->get_asset_symbol( raw_feed.feed.feed_id );
            const auto omedian_price = _chain_db->get_median_delegate_price( raw_feed.feed.feed_id );
            fc::optional<double> median_price;
            if( omedian_price )
                median_price = _chain_db->to_pretty_price_double( *omedian_price );

            result_feeds.push_back( feed_entry{ delegate_name, price, raw_feed.last_update, asset_symbol, median_price } );
        }

        return result_feeds;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("delegate_name", delegate_name) ) }

    int8_t detail::client_impl::wallet_account_set_approval( const string& account_name, int8_t approval )
    { try {
      _wallet->set_account_approval( account_name, approval );
      return _wallet->get_account_approval( account_name );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name)("approval",approval) ) }

    otransaction_record detail::client_impl::blockchain_get_transaction(const string& transaction_id, bool exact ) const
    {
      auto id = variant( transaction_id ).as<transaction_id_type>();
      return _chain_db->get_transaction(id, exact);
    }

    optional<digest_block> detail::client_impl::blockchain_get_block( const string& block )const
    {
      try
      {
          ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
          if( block.size() == 40 )
              return _chain_db->get_block_digest( block_id_type( block ) );
          else
              return _chain_db->get_block_digest( std::stoi( block ) );
      }
      catch( ... )
      {
      }
      return optional<digest_block>();
    }

    uint32_t detail::client_impl::wallet_import_bitcoin(
            const fc::path& filename,
            const string& passphrase,
            const string& account_name
            )
    {
      try
      {
          const auto count = _wallet->import_bitcoin_wallet(filename, "", account_name);
          _wallet->auto_backup( "bitcoin_import" );
          return count;
      }
      catch ( const fc::canceled_exception& )
      {
          throw;
      }
      catch( const fc::exception& e )
      {
          ilog( "import_bitcoin_wallet failed with empty password: ${e}", ("e",e.to_detail_string() ) );
      }

      const auto count = _wallet->import_bitcoin_wallet(filename, passphrase, account_name);
      _wallet->auto_backup( "bitcoin_import" );
      return count;
    }

    uint32_t detail::client_impl::wallet_import_multibit(
            const fc::path& filename,
            const string& passphrase,
            const string& account_name
            )
    {
      const auto count = _wallet->import_multibit_wallet(filename, passphrase, account_name);
      _wallet->auto_backup( "multibit_import" );
      return count;
    }

    uint32_t detail::client_impl::wallet_import_electrum(
            const fc::path& filename,
            const string& passphrase,
            const string& account_name
            )
    {
      const auto count = _wallet->import_electrum_wallet(filename, passphrase, account_name);
      _wallet->auto_backup( "electrum_import" );
      return count;
    }

    uint32_t detail::client_impl::wallet_import_armory(
            const fc::path& filename,
            const string& passphrase,
            const string& account_name
            )
    {
      const auto count = _wallet->import_armory_wallet(filename, passphrase, account_name);
      _wallet->auto_backup( "armory_import" );
      return count;
    }

    void detail::client_impl::wallet_import_keyhotee(const string& firstname,
                                                     const string& middlename,
                                                     const string& lastname,
                                                     const string& brainkey,
                                                     const string& keyhoteeid)
    {
       _wallet->import_keyhotee(firstname, middlename, lastname, brainkey, keyhoteeid);
      _wallet->auto_backup( "keyhotee_import" );
    }

    string detail::client_impl::wallet_import_private_key(const string& wif_key_to_import,
                                           const string& account_name,
                                           bool create_account,
                                           bool wallet_rescan_blockchain)
    {
      auto key = _wallet->import_wif_private_key(wif_key_to_import, account_name, create_account );
      if (wallet_rescan_blockchain)
        _wallet->scan_chain(0);

      auto oacct = _wallet->get_account_for_address( address( key ) );
      FC_ASSERT(oacct.valid(), "No account for a key we just imported" );
      _wallet->auto_backup( "key_import" );
      return oacct->name;
    }

    string detail::client_impl::wallet_dump_private_key( const std::string& input )
    {
      try
      {
          ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
          //If input is an account name...
          return utilities::key_to_wif( _wallet->get_active_private_key( input ) );
      }
      catch( ... )
      {
          try
          {
             ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
             //If input is an address...
             return utilities::key_to_wif( _wallet->get_private_key( address( input ) ) );
          }
          catch( ... )
          {
              try
              {
                 ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                 //If input is a public key...
                 return utilities::key_to_wif( _wallet->get_private_key( address( public_key_type( input ) ) ) );
              }
              catch( ... )
              {
              }
          }
      }

      return "key not found";
    }

    message detail::client_impl::wallet_mail_create(const std::string& sender,
                                                    const std::string& subject,
                                                    const std::string& body,
                                                    const message_id_type& reply_to)
    {
        return _wallet->mail_create(sender, subject, body, reply_to);
    }

    message detail::client_impl::wallet_mail_encrypt(const std::string &recipient, const message &plaintext)
    {
        auto recipient_account = _chain_db->get_account_record(recipient);
        FC_ASSERT(recipient_account, "Unknown recipient name.");

        return _wallet->mail_encrypt(recipient_account->active_key(), plaintext);
    }

    mail::message detail::client_impl::wallet_mail_open(const address& recipient, const message& ciphertext)
    {
        return _wallet->mail_open(recipient, ciphertext);
    }

    map<balance_id_type, balance_record> detail::client_impl::blockchain_list_balances( const string& first, uint32_t limit )const
    {
      return _chain_db->get_balances( first, limit );
    }

    vector<account_record> detail::client_impl::blockchain_list_accounts( const string& first, int32_t limit )const
    {
      return _chain_db->get_accounts( first, limit );
    }

    vector<account_record> detail::client_impl::blockchain_list_recently_registered_accounts()const
    {
      vector<operation> account_registrations = _chain_db->get_recent_operations(register_account_op_type);
      vector<account_record> accounts;
      accounts.reserve(account_registrations.size());

      for( const operation& op : account_registrations )
      {
        auto oaccount = _chain_db->get_account_record(op.as<register_account_operation>().owner_key);
        if(oaccount)
          accounts.push_back(*oaccount);
      }

      return accounts;
    }

    vector<asset_record> detail::client_impl::blockchain_list_assets( const string& first, int32_t limit )const
    {
      return _chain_db->get_assets( first, limit );
    }

    std::vector<fc::variant_object> detail::client_impl::network_get_peer_info( bool hide_firewalled_nodes )const
    {
      std::vector<fc::variant_object> results;
      std::vector<bts::net::peer_status> peer_statuses = _p2p_node->get_connected_peers();
      for (const bts::net::peer_status& peer_status : peer_statuses)
        if(!hide_firewalled_nodes ||
           peer_status.info["firewall_status"].as_string() == "not_firewalled")
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

    void detail::client_impl::network_add_node(const string& node, const string& command)
    {
      if (_p2p_node)
        if (command == "add")
          _self->connect_to_peer(node);
    }

    void detail::client_impl::stop()
    {
      elog( "stop...");
      _p2p_node->close();
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

    void detail::client_impl::rpc_set_username(const string& username)
    {
      std::cout << "rpc_set_username(" << username << ")\n";
      _tmp_rpc_config.rpc_user = username;
    }

    void detail::client_impl::rpc_set_password(const string& password)
    {
      std::cout << "rpc_set_password(********)\n";
      _tmp_rpc_config.rpc_password = password;
    }

    void detail::client_impl::rpc_start_server(const uint32_t port)
    {
      std::cout << "rpc_server_port: " << port << "\n";
      _tmp_rpc_config.rpc_endpoint.set_port(port);
      if(_tmp_rpc_config.is_valid())
      {
        std::cout << "starting rpc server\n";
        _rpc_server->configure_rpc(_tmp_rpc_config);
      }
      else
        std::cout << "RPC server was not started, configuration error\n";
    }

    void detail::client_impl::http_start_server(const uint32_t port)
    {
      std::cout << "http_server_port: " << port << "\n";
      _tmp_rpc_config.httpd_endpoint.set_port(port);
      if(_tmp_rpc_config.is_valid())
      {
        std::cout << "starting http server\n";
        _rpc_server->configure_http(_tmp_rpc_config);
      }
      else
        std::cout << "Http server was not started, configuration error\n";
    }

    void detail::client_impl::ntp_update_time()
    {
      FC_ASSERT(blockchain::ntp_time());
      blockchain::update_ntp_time();
    }

    void detail::client_impl::mail_store_message(const address& owner, const mail::message& message)
    {
      FC_ASSERT(_mail_server, "Mail server not enabled!");
      _mail_server->store(owner, message);
    }

    mail::inventory_type detail::client_impl::mail_fetch_inventory(const address &owner,
                                                                   const fc::time_point &start_time,
                                                                   uint32_t limit) const
    {
      FC_ASSERT(_mail_server, "Mail server not enabled!");
      return _mail_server->fetch_inventory(owner, start_time, limit);
    }

    mail::message detail::client_impl::mail_fetch_message(const mail::message_id_type& inventory_id) const
    {
      FC_ASSERT(_mail_server, "Mail server not enabled!");
      return _mail_server->fetch_message(inventory_id);
    }

    std::multimap<mail::client::mail_status, mail::message_id_type> detail::client_impl::mail_get_processing_messages() const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_processing_messages();
    }

    std::multimap<mail::client::mail_status, mail::message_id_type> detail::client_impl::mail_get_archive_messages() const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_archive_messages();
    }

    vector<mail::email_header> detail::client_impl::mail_inbox() const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_inbox();
    }

    void detail::client_impl::mail_retry_send(const message_id_type& message_id)
    {
      FC_ASSERT(_mail_client);
      _mail_client->retry_message(message_id);
    }

    void detail::client_impl::mail_remove_message(const message_id_type &message_id)
    {
      FC_ASSERT(_mail_client);
      _mail_client->remove_message(message_id);
    }

    void detail::client_impl::mail_archive_message(const message_id_type &message_id)
    {
      FC_ASSERT(_mail_client);
      _mail_client->archive_message(message_id);
    }

    int detail::client_impl::mail_check_new_messages()
    {
      FC_ASSERT(_mail_client);
      return _mail_client->check_new_messages();
    }

    mail::email_record detail::client_impl::mail_get_message(const mail::message_id_type& message_id) const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_message(message_id);
    }

    vector<mail::email_header> detail::client_impl::mail_get_messages_from(const std::string &sender) const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_messages_by_sender(sender);
    }

    vector<mail::email_header> detail::client_impl::mail_get_messages_to(const std::string &recipient) const
    {
      FC_ASSERT(_mail_client);
      return _mail_client->get_messages_by_recipient(recipient);
    }

    vector<mail::email_header> detail::client_impl::mail_get_messages_in_conversation(const std::string &account_one, const std::string &account_two) const
    {
      FC_ASSERT(_mail_client);
      auto forward = _mail_client->get_messages_from_to(account_one, account_two);
      auto backward = _mail_client->get_messages_from_to(account_two, account_one);

      std::move(backward.begin(), backward.end(), std::back_inserter(forward));
      std::sort(forward.begin(), forward.end(), [](const email_header& a, const email_header& b) {return a.timestamp < b.timestamp;});
      return forward;
    }

    mail::message_id_type detail::client_impl::mail_send(const std::string& from,
                                                         const std::string& to,
                                                         const std::string& subject,
                                                         const std::string& body,
                                                         const message_id_type& reply_to)
    {
      FC_ASSERT(_mail_client);
      return _mail_client->send_email(from, to, subject, body, reply_to);
    }

    //JSON-RPC Method Implementations END

    void client::start_networking(std::function<void()> network_started_callback)
    {
      //Start chain_downloader if there are chain_servers to connect to; otherwise, just start p2p immediately
      if( !my->_config.chain_servers.empty() )
      {
        bts::net::chain_downloader* chain_downloader = new bts::net::chain_downloader();
        for( const auto& server : my->_config.chain_servers )
          chain_downloader->add_chain_server(fc::ip::endpoint::from_string(server));
        auto download_future = chain_downloader->get_all_blocks([this](const full_block& new_block) {
          my->_chain_db->push_block(new_block);
        }, my->_chain_db->get_head_block_num() + 1);
        download_future.on_complete([this,chain_downloader,network_started_callback](const fc::exception_ptr& e) {
          if( e )
            elog("chain_downloader failed with exception: ${e}", ("e", e->to_detail_string()));
          delete chain_downloader;
          connect_to_p2p_network();
          if( network_started_callback ) network_started_callback();
        });
      }
      else
      {
        connect_to_p2p_network();
        if( network_started_callback ) network_started_callback();
      }
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
        my->_cli = new bts::cli::cli( this, nullptr, &std::cout );
        return;
      }
      // parse command-line options
      auto option_variables = parse_option_variables(argc,argv);

      fc::path datadir = bts::client::get_data_dir(option_variables);
      fc::create_directories(datadir);

      // this just clears the database if the command line says
      // TODO: rename it to smething better
      load_and_configure_chain_database(datadir, option_variables);

      fc::optional<fc::path> genesis_file_path;
      if (option_variables.count("genesis-config"))
        genesis_file_path = option_variables["genesis-config"].as<string>();

      my->_enable_ulog = option_variables["ulog"].as<bool>();
      this->open( datadir, genesis_file_path );

      if (option_variables.count("min-delegate-connection-count"))
        my->_min_delegate_connection_count = option_variables["min-delegate-connection-count"].as<uint32_t>();

      this->configure( datadir );

      if (option_variables.count("max-connections"))
      {
        my->_config.maximum_number_of_connections = option_variables["max-connections"].as<uint16_t>();
        fc::mutable_variant_object params;
        params["maximum_number_of_connections"] = my->_config.maximum_number_of_connections;
        this->network_set_advanced_node_parameters(params);
      }

      my->configure_rpc_server(my->_config,option_variables);
      my->configure_chain_server(my->_config,option_variables);

      if (option_variables.count("p2p-port"))
      {
        uint16_t p2pport = option_variables["p2p-port"].as<uint16_t>();
        listen_on_port(p2pport, option_variables.count("p2p-port") != 0);
      }
      accept_incoming_p2p_connections(option_variables["accept-incoming-connections"].as<bool>());

      // else we use the default set in bts::net::node

      //initialize cli
      if( option_variables.count("daemon") || my->_config.ignore_console )
      {
        std::cout << "Running in daemon mode, ignoring console\n";
        my->_cli = new bts::cli::cli( this, nullptr, &std::cout );
        my->_cli->set_daemon_mode(true);
      }
      else //we will accept input from the console
      {
        //if user wants us to execute a command script log for the CLI,
        //  extract the commands and put them in a temporary input stream to pass to the CLI

        if (option_variables.count("input-log"))
        {
          std::vector<string> input_logs = option_variables["input-log"].as< std::vector<string> >();
          string input_commands;
          for( const auto& input_log : input_logs )
            input_commands += extract_commands_from_log_file(input_log);
          my->_command_script_holder.reset(new std::stringstream(input_commands));
        }

        const fc::path console_log_file = datadir / "console.log";
        if( option_variables.count("log-commands") <= 0)
        {
            /* Remove any console logs for security */
            fc::remove_all( console_log_file );
            /* Don't create a log file, just output to console */
            my->_cli = new bts::cli::cli( this, my->_command_script_holder.get(), &std::cout );
        }
        else
        {
            /* Tee cli output to the console and a log file */
            ulog("Logging commands to: ${file}" ,("file",console_log_file.string()));
            my->_console_log.open(console_log_file.string());
            my->_tee_device.reset(new TeeDevice(std::cout, my->_console_log));;
            my->_tee_stream.reset(new TeeStream(*my->_tee_device.get()));

            my->_cli = new bts::cli::cli( this, my->_command_script_holder.get(), my->_tee_stream.get() );
            /* Echo command input to the log file */
            my->_cli->set_input_stream_log(my->_console_log);
        }
      } //end else we will accept input from the console


      // start listening.  this just finds a port and binds it, it doesn't start
      // accepting connections until connect_to_p2p_network()
      listen_to_p2p_network();

      if( option_variables["upnp"].as<bool>() )
      {
        ulog("Attempting to map P2P port ${port} with UPNP...",("port",get_p2p_listening_endpoint().port()));
        my->_upnp_service = std::unique_ptr<bts::net::upnp_service>(new bts::net::upnp_service);
        my->_upnp_service->map_port( get_p2p_listening_endpoint().port() );
        fc::usleep( fc::seconds(3) );
      }

      if (option_variables.count("total-bandwidth-limit"))
        get_node()->set_total_bandwidth_limit(option_variables["total-bandwidth-limit"].as<uint32_t>(),
                                              option_variables["total-bandwidth-limit"].as<uint32_t>());

      if (option_variables.count("disable-peer-advertising"))
        get_node()->disable_peer_advertising();

      if (option_variables.count("clear-peer-database"))
      {
        ulog("Erasing old peer database");
        get_node()->clear_peer_database();
      }

      if (option_variables.count("growl") || my->_config.growl_notify_endpoint)
      {
        std::string host_to_notify;
        if (option_variables.count("growl"))
          host_to_notify = option_variables["growl"].as<std::string>();
        else
          host_to_notify = *my->_config.growl_notify_endpoint;

        uint16_t port_to_notify = 23053;
        std::string::size_type colon_pos = host_to_notify.find(':');
        if (colon_pos != std::string::npos)
        {
          port_to_notify = boost::lexical_cast<uint16_t>(host_to_notify.substr(colon_pos + 1));
          host_to_notify = host_to_notify.substr(0, colon_pos);
        }

        fc::optional<std::string> growl_password;
        if (option_variables.count("growl-password"))
          growl_password = option_variables["growl-password"].as<std::string>();
        else
          growl_password = my->_config.growl_password;

        std::string bts_instance_identifier = "BitShares";
        if (option_variables.count("growl-identifier"))
          bts_instance_identifier = option_variables["growl-identifier"].as<std::string>();
        else if (my->_config.growl_bitshares_client_identifier)
          bts_instance_identifier = *my->_config.growl_bitshares_client_identifier;
        my->_notifier = std::make_shared<bts_gntp_notifier>(host_to_notify, port_to_notify, bts_instance_identifier, growl_password);
        my->_blocks_too_old_monitor_done = fc::schedule([=]() { my->blocks_too_old_monitor_task(); },
                                                        fc::time_point::now() + fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC),
                                                        "block_monitor_task");
      }

      start_networking([=]{
        fc::ip::endpoint actual_p2p_endpoint = this->get_p2p_listening_endpoint();
        std::ostringstream port_stream;
        if (actual_p2p_endpoint.get_address() == fc::ip::address())
          port_stream << "port " << actual_p2p_endpoint.port();
        else
          port_stream << (string)actual_p2p_endpoint;

        if( option_variables.count("log-commands") <= 0) /* Was breaking regression tests */
        {
          if (option_variables["accept-incoming-connections"].as<bool>())
          {
            ulog("Listening for P2P connections on ${port}",("port",port_stream.str()));
            if (option_variables.count("p2p-port"))
            {
              uint16_t p2p_port = option_variables["p2p-port"].as<uint16_t>();
              if (p2p_port != 0 && p2p_port != actual_p2p_endpoint.port())
                ulog(" (unable to bind to the desired port ${p2p_port} )", ("p2p_port",p2p_port));
            }
          }
          else
            ulog("Not accepting incoming P2P connections");
        }

        if (option_variables.count("connect-to"))
        {
          std::vector<string> hosts = option_variables["connect-to"].as<std::vector<string>>();
          for( auto peer : hosts )
            this->connect_to_peer( peer );
        }
        else if (!option_variables.count("disable-default-peers"))
        {
          for (string default_peer : my->_config.default_peers)
            this->add_node(default_peer);
        }
      });

      if (my->_config.chain_server.enabled)
      {
        my->_chain_server = std::unique_ptr<bts::net::chain_server>(
              new bts::net::chain_server(my->_chain_db,
                                         my->_config.chain_server.listen_port));
        ulog("Starting a chain server on port ${port}", ("port", my->_chain_server->get_listening_port()));
      }
    } //configure_from_command_line

    fc::future<void> client::start()
    {
      return fc::async( [=](){ my->start(); }, "client::start" );
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

    void client::accept_incoming_p2p_connections(bool accept)
    {
      my->_p2p_node->accept_incoming_connections(accept);
    }

    const config& client::configure( const fc::path& configuration_directory )
    {
      my->_data_dir = configuration_directory;
      my->_p2p_node->load_configuration( my->_data_dir );

      return my->_config;
    }

    void client::init_cli()
    {
      if( !my->_cli )
         my->_cli = new bts::cli::cli( this, nullptr, &std::cout );
    }

    void client::set_daemon_mode(bool daemon_mode)
    {
       init_cli();
       my->_cli->set_daemon_mode(daemon_mode);
    }

    fc::path client::get_data_dir()const
    {
       return my->_data_dir;
    }

    /* static */ fc::ip::endpoint client::string_to_endpoint(const std::string& remote_endpoint)
    {
      try
      {
        ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
        // first, try and parse the endpoint as a numeric_ipv4_address:port that doesn't need DNS lookup
        return fc::ip::endpoint::from_string(remote_endpoint);
      }
      catch (...)
      {
      }

      // couldn't parse as a numeric ip address, try resolving as a DNS name.  This can yield, so don't
      // do it in the catch block above
      string::size_type colon_pos = remote_endpoint.find(':');
      try
      {
        uint16_t port = boost::lexical_cast<uint16_t>( remote_endpoint.substr( colon_pos + 1, remote_endpoint.size() ) );

        string hostname = remote_endpoint.substr( 0, colon_pos );
        std::vector<fc::ip::endpoint> endpoints = fc::resolve(hostname, port);
        if ( endpoints.empty() )
          FC_THROW_EXCEPTION(fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname));
        return endpoints.back();
      }
      catch (const boost::bad_lexical_cast&)
      {
        FC_THROW("Bad port: ${port}", ("port", remote_endpoint.substr( colon_pos + 1, remote_endpoint.size() )));
      }
    }

    void client::add_node( const string& remote_endpoint )
    {
      fc::ip::endpoint endpoint;
      fc::oexception string_to_endpoint_error;
      try
      {
        endpoint = string_to_endpoint(remote_endpoint);
      }
      catch (const fc::exception& e)
      {
        string_to_endpoint_error = e;
      }
      if (string_to_endpoint_error)
      {
        ulog("Unable to add peer ${remote_endpoint}: ${error}",
             ("remote_endpoint", remote_endpoint)("error", string_to_endpoint_error->to_string()));
        return;
      }

      try
      {
        ulog("Adding peer ${peer} to peer database", ("peer", endpoint));
        my->_p2p_node->add_node(endpoint);
      }
      catch (const bts::net::already_connected_to_requested_peer&)
      {
      }
    }
    void client::connect_to_peer(const string& remote_endpoint)
    {
      fc::ip::endpoint endpoint;
      fc::oexception string_to_endpoint_error;
      try
      {
        endpoint = string_to_endpoint(remote_endpoint);
      }
      catch (const fc::exception& e)
      {
        string_to_endpoint_error = e;
      }
      if (string_to_endpoint_error)
      {
        ulog("Unable to initiate connection to peer ${remote_endpoint}: ${error}",
             ("remote_endpoint", remote_endpoint)("error", string_to_endpoint_error->to_string()));
        return;
      }

      try
      {
        ulog("Attempting to connect to peer ${peer}", ("peer", endpoint));
        my->_p2p_node->connect_to(endpoint);
      }
      catch (const bts::net::already_connected_to_requested_peer&)
      {
      }
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

    bool client::handle_message(const bts::net::message& message, bool sync_mode)
    {
      return my->handle_message(message, sync_mode);
    }
    void client::sync_status(uint32_t item_type, uint32_t item_count)
    {
      my->sync_status(item_type, item_count);
    }

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
       _wallet->auto_backup( "account_add" );
    }

    public_key_type client_impl::wallet_account_create( const string& account_name,
                                                        const variant& private_data )
    {
       const auto result = _wallet->create_account( account_name, private_data );
       _wallet->auto_backup( "account_create" );
       return result;
    }

    void client_impl::wallet_account_set_favorite( const string& account_name, bool is_favorite )
    {
        _wallet->account_set_favorite( account_name, is_favorite );
    }

    void client_impl::debug_enable_output(bool enable_flag)
    {
      _cli->enable_output(enable_flag);
    }

    void client_impl::debug_filter_output_for_tests(bool enable_flag)
    {
      _cli->filter_output_for_tests(enable_flag);
    }

    void client_impl::debug_update_logging_config()
    {
      config temp_config = load_config( _data_dir, _enable_ulog );
      fc::configure_logging( temp_config.logging );
      // re-register the _user_appender which was overwritten by configure_logging()
      fc::logger::get("user").add_appender(_user_appender);
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

    variant_object client_impl::blockchain_get_info() const
    {
       auto info = fc::mutable_variant_object();

       info["blockchain_id"]                = _chain_db->chain_id();

       info["symbol"]                       = BTS_BLOCKCHAIN_SYMBOL;
       info["name"]                         = BTS_BLOCKCHAIN_NAME;
       info["version"]                      = BTS_BLOCKCHAIN_VERSION;
       info["db_version"]                   = BTS_BLOCKCHAIN_DATABASE_VERSION;
       info["genesis_timestamp"]            = _chain_db->get_genesis_timestamp();

       info["block_interval"]               = BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
       info["max_block_size"]               = BTS_BLOCKCHAIN_MAX_BLOCK_SIZE;
       info["max_blockchain_size"]          = BTS_BLOCKCHAIN_MAX_SIZE;

       info["address_prefix"]               = BTS_ADDRESS_PREFIX;
       info["inactivity_fee_apr"]           = BTS_BLOCKCHAIN_INACTIVE_FEE_APR;
       info["relay_fee"]                    = _chain_db->get_relay_fee();

       info["delegate_num"]                 = BTS_BLOCKCHAIN_NUM_DELEGATES;

       info["name_size_max"]                = BTS_BLOCKCHAIN_MAX_NAME_SIZE;
       info["memo_size_max"]                = BTS_BLOCKCHAIN_MAX_MEMO_SIZE;
       info["data_size_max"]                = BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE;

       info["symbol_size_max"]              = BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE;
       info["symbol_size_min"]              = BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE;
       info["asset_reg_fee"]                = _chain_db->get_asset_registration_fee();
       info["asset_shares_max"]             = BTS_BLOCKCHAIN_MAX_SHARES;

       info["max_pending_queue_size"]       = BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
       info["max_trx_per_second"]           = BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND;

       return info;
    }

    variant_object client_impl::get_info()const
    {
      const auto now = blockchain::now();
      auto info = fc::mutable_variant_object();

      /* Blockchain */
      const auto head_block_num                                 = _chain_db->get_head_block_num();
      info["blockchain_head_block_num"]                         = head_block_num;
      info["blockchain_head_block_age"]                         = variant();
      info["blockchain_head_block_timestamp"]                   = variant();
      time_point_sec head_block_timestamp;
      if( head_block_num > 0 )
      {
          head_block_timestamp                                 = _chain_db->now();
          info["blockchain_head_block_age"]                    = ( now - head_block_timestamp ).to_seconds();
          info["blockchain_head_block_timestamp"]              = head_block_timestamp;
      }

      info["blockchain_average_delegate_participation"]         = variant();
      const auto participation                                  = _chain_db->get_average_delegate_participation();
      if( participation <= 100 )
          info["blockchain_average_delegate_participation"]     = participation;

      info["blockchain_confirmation_requirement"]               = _chain_db->get_required_confirmations();

      info["blockchain_share_supply"]                           = variant();
      const auto share_record                                   = _chain_db->get_asset_record( BTS_ADDRESS_PREFIX );
      if( share_record.valid() )
          info["blockchain_share_supply"]                       = share_record->current_share_supply;

      const auto blocks_left                                    = BTS_BLOCKCHAIN_NUM_DELEGATES - (head_block_num % BTS_BLOCKCHAIN_NUM_DELEGATES);
      info["blockchain_blocks_left_in_round"]                   = blocks_left;

      info["blockchain_next_round_time"]                        = variant();
      info["blockchain_next_round_timestamp"]                   = variant();
      if( head_block_num > 0 )
      {
          const auto current_round_timestamp                    = blockchain::get_slot_start_time( now );
          const auto next_round_timestamp                       = current_round_timestamp + (blocks_left * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
          info["blockchain_next_round_time"]                    = ( next_round_timestamp - now ).to_seconds();
          info["blockchain_next_round_timestamp"]               = next_round_timestamp;
      }

      info["blockchain_random_seed"]                            = _chain_db->get_current_random_seed();

      /* Client */
      info["client_data_dir"]                                   = fc::absolute( _data_dir );
      //info["client_httpd_port"]                                 = _config.is_valid() ? _config.httpd_endpoint.port() : 0;
      //info["client_rpc_port"]                                   = _config.is_valid() ? _config.rpc_endpoint.port() : 0;
      info["client_version"]                                    = bts::client::version_info()["client_version"].as_string();

      /* Network */
      info["network_num_connections"]                           = network_get_connection_count();
      fc::variant_object advanced_params                        = network_get_advanced_node_parameters();
      info["network_num_connections_max"]                       = advanced_params["maximum_number_of_connections"];

      /* NTP */
      info["ntp_time"]                                          = variant();
      info["ntp_time_error"]                                    = variant();
      if( blockchain::ntp_time().valid() )
      {
        info["ntp_time"]                                        = now;
        info["ntp_time_error"]                                  = static_cast<double>(blockchain::ntp_error().count()) / fc::seconds(1).count();
      }

      /* Wallet */
      const auto is_open                                        = _wallet->is_open();
      info["wallet_open"]                                       = is_open;

      info["wallet_unlocked"]                                   = variant();
      info["wallet_unlocked_until"]                             = variant();
      info["wallet_unlocked_until_timestamp"]                   = variant();

      info["wallet_last_scanned_block_timestamp"]               = variant();
      info["wallet_scan_progress"]                              = variant();

      info["wallet_block_production_enabled"]                   = variant();
      info["wallet_next_block_production_time"]                 = variant();
      info["wallet_next_block_production_timestamp"]            = variant();

      if( is_open )
      {
        info["wallet_unlocked"]                                 = _wallet->is_unlocked();

        const auto unlocked_until                               = _wallet->unlocked_until();
        if( unlocked_until.valid() )
        {
          info["wallet_unlocked_until"]                         = ( *unlocked_until - now ).to_seconds();
          info["wallet_unlocked_until_timestamp"]               = *unlocked_until;

          const auto last_scanned_block_num                     = _wallet->get_last_scanned_block_number();
          if( last_scanned_block_num > 0 )
          {
              try
              {
                  info["wallet_last_scanned_block_timestamp"]   = _chain_db->get_block_header( last_scanned_block_num ).timestamp;
              }
              catch( ... )
              {
              }
          }

          info["wallet_scan_progress"]                          = _wallet->get_scan_progress();

          const auto enabled_delegates                          = _wallet->get_my_delegates( enabled_delegate_status );
          const auto block_production_enabled                   = !enabled_delegates.empty();
          info["wallet_block_production_enabled"]               = block_production_enabled;

          if( block_production_enabled )
          {
            const auto next_block_timestamp                     = _wallet->get_next_producible_block_timestamp( enabled_delegates );
            if( next_block_timestamp.valid() )
            {
              info["wallet_next_block_production_time"]         = ( *next_block_timestamp - now ).to_seconds();
              info["wallet_next_block_production_timestamp"]    = *next_block_timestamp;
            }
          }
        }
      }

      return info;
    }

    asset client_impl::blockchain_calculate_supply( const string& asset )const
    {
       asset_id_type asset_id;
       if( std::all_of( asset.begin(), asset.end(), ::isdigit ) )
           asset_id = std::stoi( asset );
       else
           asset_id = _chain_db->get_asset_id( asset );

       return _chain_db->calculate_supply( asset_id );
    }

    asset client_impl::blockchain_calculate_debt( const string& asset )const
    {
       asset_id_type asset_id;
       if( std::all_of( asset.begin(), asset.end(), ::isdigit ) )
           asset_id = std::stoi( asset );
       else
           asset_id = _chain_db->get_asset_id( asset );

       return _chain_db->calculate_debt( asset_id );
    }

    void client_impl::wallet_rescan_blockchain( uint32_t start, uint32_t count, bool fast_scan )
    { try {
       _wallet->scan_chain( start, start + count, fast_scan );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("start",start)("count",count) ) }

    wallet_transaction_record client_impl::wallet_scan_transaction( const string& transaction_id, bool overwrite_existing )
    { try {
       return _wallet->scan_transaction( transaction_id, overwrite_existing );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

    void client_impl::wallet_scan_transaction_experimental( const string& transaction_id, bool overwrite_existing )
    { try {
#ifdef BTS_TEST_NETWORK
       _wallet->scan_transaction_experimental( transaction_id, overwrite_existing );
#endif
    } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

    wallet_transaction_record client_impl::wallet_get_transaction( const string& transaction_id )
    { try {
       return _wallet->get_transaction( transaction_id );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

    bts::blockchain::blockchain_security_state client_impl::blockchain_get_security_state()const
    {
        blockchain_security_state state;
        int64_t required_confirmations = _chain_db->get_required_confirmations();
        double participation_rate = _chain_db->get_average_delegate_participation();
        if( participation_rate > 100 ) participation_rate = 100;

        state.estimated_confirmation_seconds = (uint32_t)(required_confirmations * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
        state.participation_rate = participation_rate;
        if (!blockchain_is_synced())
        {
            state.alert_level = bts::blockchain::blockchain_security_state::grey;
        }
        else if (required_confirmations <= BTS_BLOCKCHAIN_NUM_DELEGATES / 2
            && participation_rate > 80)
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
                                                                    const share_type& delegate_pay_rate )
    { try {
        const auto record = _wallet->register_account( account_name, data, delegate_pay_rate, pay_with_account );
        network_broadcast_transaction( record.trx );
        return record;
    } FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("data", data)) }

    variant_object client_impl::wallet_get_info()
    {
       return _wallet->get_info().get_object();
    }

    void client_impl::wallet_account_update_private_data( const string& account_to_update,
                                                          const variant& private_data )
    {
       _wallet->update_account_private_data(account_to_update, private_data);
    }

    wallet_transaction_record client_impl::wallet_account_update_registration(
            const string& account_to_update,
            const string& pay_from_account,
            const variant& public_data,
            const share_type& delegate_pay_rate )
    {
       const auto record = _wallet->update_registered_account( account_to_update, pay_from_account, public_data, delegate_pay_rate );
       network_broadcast_transaction( record.trx );
       return record;
    }

    wallet_transaction_record detail::client_impl::wallet_account_update_active_key( const std::string& account_to_update,
                                                                                     const std::string& pay_from_account,
                                                                                     const std::string& new_active_key )
    {
       const auto record = _wallet->update_active_key( account_to_update, pay_from_account, new_active_key );
       network_broadcast_transaction( record.trx );
       return record;
    }

    fc::variant_object client_impl::network_get_info() const
    {
      return _p2p_node->network_get_info();
    }

    fc::variant_object client_impl::network_get_usage_stats() const
    {
      return _p2p_node->network_get_usage_stats();
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

    void client_impl::debug_wait(uint32_t wait_time) const
    {
      fc::usleep(fc::seconds(wait_time));
    }

    void client_impl::debug_wait_block_interval(uint32_t wait_time) const
    {
      fc::usleep(fc::seconds(wait_time*BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC));
    }

    vector<public_key_summary> client_impl::wallet_account_list_public_keys( const string& account_name )
    {
        vector<public_key_summary> summaries;
        vector<public_key_type> keys = _wallet->get_public_keys_in_account( account_name );
        summaries.reserve( keys.size() );
        for( const auto& key : keys )
        {
            summaries.push_back(_wallet->get_public_key_summary( key ));
        }
        return summaries;
    }

   account_balance_summary_type client_impl::wallet_account_balance( const string& account_name )const
   {
      return _wallet->get_account_balances( account_name );
   }

   account_balance_id_summary_type client_impl::wallet_account_balance_ids( const string& account_name )const
   {
      return _wallet->get_account_balance_ids( account_name );
   }

   account_balance_summary_type client_impl::wallet_account_yield( const string& account_name )const
   {
      return _wallet->get_account_yield( account_name );
   }

   wallet_transaction_record client_impl::wallet_market_submit_bid(
           const string& from_account,
           double quantity,
           const string& quantity_symbol,
           double quote_price,
           const string& quote_symbol,
           bool allow_stupid_bid )
   {
      vector<market_order> lowest_ask = blockchain_market_order_book(quote_symbol, quantity_symbol, 1).second;

      if (!allow_stupid_bid && lowest_ask.size()
          && fc::variant(quote_price).as_double() > _chain_db->to_pretty_price_double(lowest_ask.front().get_price()) * 1.05)
        FC_THROW_EXCEPTION(stupid_order, "You are attempting to bid at more than 5% above the buy price. "
                                         "This bid is based on economically unsound principles, and is ill-advised. "
                                         "If you're sure you want to do this, place your bid again and set allow_stupid_bid to true.");

      const auto record = _wallet->submit_bid( from_account, quantity, quantity_symbol, quote_price, quote_symbol );
      network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_market_submit_ask(
               const string& from_account,
               double quantity,
               const string& quantity_symbol,
               double quote_price,
               const string& quote_symbol,
               bool allow_stupid_ask )
   {
      vector<market_order> highest_bid = blockchain_market_order_book(quote_symbol, quantity_symbol, 1).first;

      if (!allow_stupid_ask && highest_bid.size()
          && fc::variant(quote_price).as_double() < _chain_db->to_pretty_price_double(highest_bid.front().get_price()) * .95)
        FC_THROW_EXCEPTION(stupid_order, "You are attempting to ask at more than 5% below the buy price. "
                                         "This ask is based on economically unsound principles, and is ill-advised. "
                                         "If you're sure you want to do this, place your ask again and set allow_stupid_ask to true.");

      const auto record = _wallet->submit_ask( from_account, quantity, quantity_symbol, quote_price, quote_symbol );
      network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_market_submit_short(
           const string& from_account,
           double quantity,
           const string& quote_symbol,
           double collateral_ratio,
           const string& collateral_symbol,
           double short_price_limit )
   {
      const auto record = _wallet->submit_short( from_account,
                                                 quantity,
                                                 quote_symbol,
                                                 collateral_ratio,
                                                 collateral_symbol,
                                                 short_price_limit );
      network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_market_batch_update(const std::vector<order_id_type> &cancel_order_ids,
                                                                     const std::vector<order_description> &new_orders,
                                                                     bool sign)
   {
      const auto record = _wallet->batch_market_update(cancel_order_ids, new_orders, sign);
      if( sign )
         network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_market_cover(
           const string& from_account,
           double quantity,
           const string& quantity_symbol,
           const order_id_type& cover_id )
   {
      const auto record = _wallet->cover_short( from_account, quantity, quantity_symbol, cover_id );
      network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_delegate_withdraw_pay( const string& delegate_name,
                                                                        const string& to_account_name,
                                                                        double amount_to_withdraw )
   {
      const auto record = _wallet->withdraw_delegate_pay( delegate_name, amount_to_withdraw, to_account_name );
      network_broadcast_transaction( record.trx );
      return record;
   }

   asset client_impl::wallet_set_transaction_fee( double fee )
   { try {
      oasset_record asset_record = _chain_db->get_asset_record( asset_id_type() );
      FC_ASSERT( asset_record.valid() );
      _wallet->set_transaction_fee( asset( fee * asset_record->precision ) );
      return _wallet->get_transaction_fee();
   } FC_CAPTURE_AND_RETHROW( (fee) ) }

   asset client_impl::wallet_get_transaction_fee( const string& fee_symbol )
   {
      if( fee_symbol.empty() )
         return _wallet->get_transaction_fee( _chain_db->get_asset_id( BTS_BLOCKCHAIN_SYMBOL ) );
      return _wallet->get_transaction_fee( _chain_db->get_asset_id( fee_symbol ) );
   }

   bool client_impl::blockchain_is_synced() const
   {
     return (blockchain::now() - _chain_db->get_head_block().timestamp) < fc::seconds(BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
   }

   vector<market_order>    client_impl::blockchain_market_list_bids( const string& quote_symbol,
                                                                       const string& base_symbol,
                                                                       uint32_t limit  )
   {
      return _chain_db->get_market_bids( quote_symbol, base_symbol, limit );
   }
   vector<market_order>    client_impl::blockchain_market_list_asks( const string& quote_symbol,
                                                                       const string& base_symbol,
                                                                       uint32_t limit  )
   {
      return _chain_db->get_market_asks( quote_symbol, base_symbol, limit );
   }

   vector<market_order>    client_impl::blockchain_market_list_shorts( const string& quote_symbol,
                                                                       uint32_t limit  )const
   {
      return _chain_db->get_market_shorts( quote_symbol, limit );
   }
   vector<market_order>    client_impl::blockchain_market_list_covers( const string& quote_symbol,
                                                                       uint32_t limit  )
   {
      return _chain_db->get_market_covers( quote_symbol, limit );
   }

   share_type              client_impl::blockchain_market_get_asset_collateral( const string& symbol )
   {
      return _chain_db->get_asset_collateral( symbol );
   }

   std::pair<vector<market_order>,vector<market_order>> client_impl::blockchain_market_order_book( const string& quote_symbol,
                                                                                              const string& base_symbol,
                                                                                              uint32_t limit  )
   {
      auto bids = blockchain_market_list_bids(quote_symbol, base_symbol, limit);
      auto asks = blockchain_market_list_asks(quote_symbol, base_symbol, limit);
      auto covers = blockchain_market_list_covers(quote_symbol,limit);
      asks.insert( asks.end(), covers.begin(), covers.end() );

      std::sort(bids.rbegin(), bids.rend(), [](const market_order& a, const market_order& b) -> bool {
        return a.market_index < b.market_index;
      });

      return std::make_pair(bids, asks);
   }

   std::vector<order_history_record> client_impl::blockchain_market_order_history( const std::string &quote_symbol,
                                                                                   const std::string &base_symbol,
                                                                                   uint32_t skip_count,
                                                                                   uint32_t limit,
                                                                                   const string& owner )const
   {
       auto quote_id = _chain_db->get_asset_id(quote_symbol);
       auto base_id = _chain_db->get_asset_id(base_symbol);
       address owner_address = owner.empty()? address() : address(owner);

       return _chain_db->market_order_history(quote_id, base_id, skip_count, limit, owner_address);
   }

   market_history_points client_impl::blockchain_market_price_history( const std::string& quote_symbol,
                                                                       const std::string& base_symbol,
                                                                       const fc::time_point& start_time,
                                                                       const fc::microseconds& duration,
                                                                       const market_history_key::time_granularity_enum& granularity )const
   {
      return _chain_db->get_market_price_history( _chain_db->get_asset_id(quote_symbol),
                                                  _chain_db->get_asset_id(base_symbol),
                                                  start_time, duration, granularity );
   }

   wallet_transaction_record client_impl::wallet_market_add_collateral( const std::string &from_account_name,
                                                                        const order_id_type &cover_id,
                                                                        const share_type &collateral_to_add )
   {
      const auto record = _wallet->add_collateral( from_account_name, cover_id, collateral_to_add );
      network_broadcast_transaction( record.trx );
      return record;
   }

   map<order_id_type, market_order> client_impl::wallet_account_order_list( const string& account_name,
                                                                            uint32_t limit )
   {
      return _wallet->get_market_orders( account_name, limit );
   }

   map<order_id_type, market_order> client_impl::wallet_market_order_list( const string& quote_symbol,
                                                                           const string& base_symbol,
                                                                           uint32_t limit,
                                                                           const string& account_name )
   {
      return _wallet->get_market_orders( quote_symbol, base_symbol, limit, account_name );
   }

   wallet_transaction_record client_impl::wallet_market_cancel_order( const order_id_type& order_id )
   {
      const auto record = _wallet->cancel_market_orders( {order_id} );
      network_broadcast_transaction( record.trx );
      return record;
   }

   wallet_transaction_record client_impl::wallet_market_cancel_orders( const vector<order_id_type>& order_ids )
   {
      const auto record = _wallet->cancel_market_orders( order_ids );
      network_broadcast_transaction( record.trx );
      return record;
   }

   account_vote_summary_type client_impl::wallet_account_vote_summary( const string& account_name )const
   {
      if( !account_name.empty() && !_chain_db->is_valid_account_name( account_name ) )
          FC_CAPTURE_AND_THROW( invalid_account_name, (account_name) );

      return _wallet->get_account_vote_summary( account_name );
   }

   map<transaction_id_type, transaction_record> client_impl::blockchain_get_block_transactions( const string& block )const
   {
      vector<transaction_record> transactions;
      if( block.size() == 40 )
        transactions = _chain_db->get_transactions_for_block( block_id_type( block ) );
      else
        transactions = _chain_db->get_transactions_for_block( _chain_db->get_block_id( std::stoi( block ) ) );

      map<transaction_id_type, transaction_record> transactions_map;
      for( const auto& transaction : transactions )
          transactions_map[ transaction.trx.id() ] = transaction;

      return transactions_map;
   }

   map<fc::time_point, fc::exception> client_impl::debug_list_errors( int32_t first_error_number, uint32_t limit, const string& filename )const
   {
      map<fc::time_point, fc::exception> result;
      int count = 0;
      auto itr = _exception_db.begin();
      while( itr.valid() )
      {
         ++count;
         if( count > first_error_number )
         {
            result[itr.key()] = itr.value();
            if (--limit == 0)
                break;
         }
         ++itr;
      }

      if( filename != "" )
      {
          fc::path file_path( filename );
          FC_ASSERT( !fc::exists( file_path ) );
          std::ofstream out( filename.c_str() );
          for( auto item : result )
          {
             out << std::string(item.first) << "  " << item.second.to_detail_string() <<"\n-----------------------------\n";
          }
      }
      return result;
   }

   map<fc::time_point, std::string> client_impl::debug_list_errors_brief( int32_t first_error_number, uint32_t limit, const string& filename ) const
   {
      map<fc::time_point, fc::exception> full_errors = debug_list_errors(first_error_number, limit, "");

      map<fc::time_point, std::string> brief_errors;
      for (auto full_error : full_errors)
        brief_errors.insert(std::make_pair(full_error.first, full_error.second.what()));

      if( filename != "" )
      {
          fc::path file_path( filename );
          FC_ASSERT( !fc::exists( file_path ) );
          std::ofstream out( filename.c_str() );
          for( auto item : brief_errors )
             out << std::string(item.first) << "  " << item.second <<"\n";
      }

      return brief_errors;
   }

   void client_impl::debug_clear_errors( const fc::time_point& start_time, int32_t first_error_number, uint32_t limit )
   {
      auto itr = _exception_db.lower_bound( start_time );
      //skip to first error to clear
      while (itr.valid() && first_error_number > 1)
      {
        --first_error_number;
        ++itr;
      }
      //clear the desired errors
      while( itr.valid() && limit > 0)
      {
        _exception_db.remove(itr.key());
        --limit;
        ++itr;
      }
   }

   void client_impl::debug_write_errors_to_file( const string& path, const fc::time_point& start_time )const
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

  fc::variant_object client_impl::debug_get_call_statistics() const
  {
    return _p2p_node->get_call_statistics();
  }

  fc::variant_object client_impl::debug_verify_delegate_votes() const
  {
    return _chain_db->find_delegate_vote_discrepancies();
  }


  vote_summary   client_impl::wallet_check_vote_proportion( const string& account_name )
  {
      return _wallet->get_vote_proportion( account_name );
  }

   std::string client_impl::blockchain_export_fork_graph( uint32_t start_block, uint32_t end_block, const std::string& filename )const
   {
      return _chain_db->export_fork_graph( start_block, end_block, filename );
   }

   std::map<uint32_t, vector<fork_record>> client_impl::blockchain_list_forks()const
   {
      return _chain_db->get_forks_list();
   }

   vector<slot_record> client_impl::blockchain_get_delegate_slot_records( const string& delegate_name )const
   {
      auto delegate_record = _chain_db->get_account_record( delegate_name );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate(), "${n} is not a delegate!", ("n",delegate_name) );
      return _chain_db->get_delegate_slot_records( delegate_record->id );
   }

   string client_impl::blockchain_get_block_signee( const string& block )const
   {
      if( block.size() == 40 )
          return _chain_db->get_block_signee( block_id_type( block ) ).name;
      else
          return _chain_db->get_block_signee( std::stoi( block ) ).name;
   }

   bool client_impl::blockchain_verify_signature(const string& signing_account, const fc::sha256& hash, const fc::ecc::compact_signature& signature) const
   {
      oaccount_record rec = blockchain_get_account(signing_account);
      if (!rec.valid())
        return false;

      // logic shamelessly copy-pasted from signed_block_header::validate_signee()
      // NB LHS of == operator is bts::blockchain::public_key_type and RHS is fc::ecc::public_key,
      //   the opposite order won't compile (look at operator== prototype in public_key_type class declaration)
      return rec->active_key() == fc::ecc::public_key(signature, hash);
   }

   void client_impl::debug_start_simulated_time(const fc::time_point& starting_time)
   {
     bts::blockchain::start_simulated_time(starting_time);
   }

   void client_impl::debug_advance_time(int32_t delta_time, const std::string& unit /* = "seconds" */)
   {
     if (unit == "blocks")
       delta_time *= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
     else if (unit == "rounds")
       delta_time *= BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
     else if (unit != "seconds")
       FC_THROW_EXCEPTION(fc::invalid_arg_exception, "unit must be \"seconds\", \"blocks\", or \"rounds\", was: \"${unit}\"", ("unit", unit));
     bts::blockchain::advance_time(delta_time);
   }

   void client_impl::debug_wait_for_block_by_number(uint32_t block_number, const std::string& type /* = "absolute" */)
   {
      if (type == "relative")
        block_number += _chain_db->get_head_block_num();
      else if (type != "absolute")
        FC_THROW_EXCEPTION(fc::invalid_arg_exception, "type must be \"absolute\", or \"relative\", was: \"${type}\"", ("type", type));
      if (_chain_db->get_head_block_num() >= block_number)
        return;
      fc::promise<void>::ptr block_arrived_promise(new fc::promise<void>("debug_wait_for_block_by_number"));
      class wait_for_block : public bts::blockchain::chain_observer
      {
        uint32_t               _block_number;
        fc::promise<void>::ptr _completion_promise;
      public:
        wait_for_block(uint32_t block_number, fc::promise<void>::ptr completion_promise) :
          _block_number(block_number),
          _completion_promise(completion_promise)
        {}
        void state_changed(const pending_chain_state_ptr& state) override {}
        void block_applied(const block_summary& summary) override
        {
          if (summary.block_data.block_num >= _block_number)
            _completion_promise->set_value();
        }
      };
      wait_for_block block_waiter(block_number, block_arrived_promise);
      _chain_db->add_observer(&block_waiter);
      try
      {
        block_arrived_promise->wait();
      }
      catch (...)
      {
        _chain_db->remove_observer(&block_waiter);
        throw;
      }
      _chain_db->remove_observer(&block_waiter);
   }

   void client_impl::wallet_delegate_set_block_production( const string& delegate_name, bool enabled )
   {
      _wallet->set_delegate_block_production( delegate_name, enabled );
      reschedule_delegate_loop();
   }

   bool client_impl::wallet_set_transaction_scanning( bool enabled )
   {
       _wallet->set_transaction_scanning( enabled );
       return _wallet->get_transaction_scanning();
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

   fc::ecc::compact_signature client_impl::wallet_sign_hash(const string& signing_account, const fc::sha256& hash)
   {
      return _wallet->get_active_private_key(signing_account).sign_compact(hash);
   }

   std::string client_impl::wallet_login_start(const std::string &server_account)
   {
      return _wallet->login_start(server_account);
   }

   fc::variant client_impl::wallet_login_finish(const public_key_type &server_key, const public_key_type &client_key, const fc::ecc::compact_signature &client_signature)
   {
      return _wallet->login_finish(server_key, client_key, client_signature);
   }

   vector<bts::blockchain::market_transaction> client_impl::blockchain_list_market_transactions( uint32_t block_num )const
   {
      return _chain_db->get_market_transactions( block_num );
   }

   bts::blockchain::api_market_status client_impl::blockchain_market_status( const std::string& quote,
                                                                         const std::string& base )const
   {
      auto qrec = _chain_db->get_asset_record(quote);
      auto brec = _chain_db->get_asset_record(base);
      FC_ASSERT( qrec && brec );
      auto oresult = _chain_db->get_market_status( qrec->id, brec->id );
      FC_ASSERT( oresult );

      api_market_status result(*oresult);
      if( oresult->center_price.ratio == fc::uint128() )
      {
        oprice median_delegate_price = _chain_db->get_median_delegate_price(qrec->id);
        result.center_price = _chain_db->to_pretty_price_double(median_delegate_price? *median_delegate_price : price());
      }
      else
        result.center_price = _chain_db->to_pretty_price_double(oresult->center_price);
      return result;
   }

   bts::blockchain::asset client_impl::blockchain_unclaimed_genesis() const
   {
        return _chain_db->unclaimed_genesis();
   }

   wallet_transaction_record client_impl::wallet_publish_price_feed( const std::string& delegate_account,
                                                                     double real_amount_per_xts,
                                                                     const std::string& real_amount_symbol )
   {
      const auto record = _wallet->publish_price( delegate_account, real_amount_per_xts, real_amount_symbol );
      network_broadcast_transaction( record.trx );
      return record;
   }
   wallet_transaction_record client_impl::wallet_publish_feeds( const std::string& delegate_account,
                                                                const map<string,double>& real_amount_per_xts )
   {
      const auto record = _wallet->publish_feeds( delegate_account, real_amount_per_xts );
      network_broadcast_transaction( record.trx );
      return record;
   }

   vector<burn_record> client_impl::blockchain_get_account_wall( const string& account )const
   {
      return _chain_db->fetch_burn_records( account );
   }

   int32_t client_impl::wallet_regenerate_keys( const std::string& account, uint32_t number_to_regenerate )
   {
      return _wallet->regenerate_keys( account, number_to_regenerate );
   }

   } // namespace detail
///////////////////////////////////////////////////////////////////////////////////////////////


   bts::api::common_api* client::get_impl() const
   {
     return my.get();
   }

  bool rpc_server_config::is_valid() const
  {
#ifndef _WIN32
    if (rpc_user.empty())
      return false;
    if (rpc_password.empty())
      return false;
#endif
    return true;
  }

} } // bts::client
