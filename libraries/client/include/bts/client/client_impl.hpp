#pragma once

#include <bts/cli/cli.hpp>
#include <bts/client/notifier.hpp>
#include <bts/net/chain_server.hpp>
#include <bts/net/upnp.hpp>

#include <fc/log/appender.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/program_options.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/tee.hpp>

#include <fstream>
#include <iostream>

namespace bts { namespace client {

typedef boost::iostreams::tee_device<std::ostream, std::ofstream> TeeDevice;
typedef boost::iostreams::stream<TeeDevice> TeeStream;

namespace detail {
using namespace boost;

using namespace bts::wallet;
using namespace bts::blockchain;

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
            return _thread->async([=](){ log(m); }, "user_appender::log").wait();

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
            return _thread->async([=](){ return get_history(); }, "user_appender::get_history").wait();
         return _history;
      }

      void clear_history()
      {
         if (!_thread->is_current())
            return _thread->async([=](){ return clear_history(); }, "user_appender::clear_history").wait();
         _history.clear();
      }


   private:
      // TODO: consider a deque and enforce maximum length?
      vector<string>                       _history;
      client_impl&                         _client_impl;
      fc::thread*                          _thread;
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
      if( _chain_downloader_future.valid() && !_chain_downloader_future.ready() )
         _chain_downloader_future.cancel_and_wait(__FUNCTION__);
      _rpc_server.reset(); // this needs to shut down before the _p2p_node because several RPC requests will try to dereference _p2p_node.  Shutting down _rpc_server kills all active/pending requests
      delete _cli;
      _cli = nullptr;
      _p2p_node.reset();
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
   void set_client_debug_name(const string& name)
   {
      _config.client_debug_name = name;
      return;
   }

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
      return _chain_db->get_chain_id();
   }
   virtual std::vector<bts::net::item_hash_t> get_blockchain_synopsis(uint32_t item_type,
                                                                      const bts::net::item_hash_t& reference_point = bts::net::item_hash_t(),
                                                                      uint32_t number_of_blocks_after_reference_point = 0) override;
   virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
   virtual void connection_count_changed(uint32_t c) override;
   virtual uint32_t get_block_number(const bts::net::item_hash_t& block_id) override;
   virtual fc::time_point_sec get_block_time(const bts::net::item_hash_t& block_id) override;
   virtual fc::time_point_sec get_blockchain_now() override;
   virtual bts::net::item_hash_t get_head_block_id() const override;
   virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override;
   virtual void error_encountered(const std::string& message, const fc::oexception& error) override;
   /// @}

   bts::client::client*                                    _self;
   std::string                                             _user_agent;
   bts::cli::cli*                                          _cli = nullptr;

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
   fc::time_point                                          _last_sync_status_message_time;
   bool                                                    _last_sync_status_message_indicated_in_sync;
   uint32_t                                                _last_sync_status_head_block;
   uint32_t                                                _remaining_items_to_sync;
   boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> > _sync_speed_accumulator;

   // Chain downloader
   fc::future<void>                                        _chain_downloader_future;
   bool                                                    _chain_downloader_running = false;
   uint32_t                                                _chain_downloader_blocks_remaining = 0;

   fc::time_point                                          _last_connection_count_message_time;
   /** display messages about the connection count changing at most once every _connection_count_notification_interval */
   fc::microseconds                                        _connection_count_notification_interval;
   /** exception: if you have fewer than _connection_count_always_notify_threshold connections, notify any time the count changes */
   uint32_t                                                _connection_count_always_notify_threshold;
   uint32_t                                                _connection_count_last_value_displayed;

   config                                                  _config;

   // Delegate block production
   fc::future<void>                                        _delegate_loop_complete;
   bool                                                    _delegate_loop_first_run = true;
   delegate_config                                         _delegate_config;

   //start by assuming not syncing, network won't send us a msg if we start synced and stay synched.
   //at worst this means we might briefly sending some pending transactions while not synched.
   bool                                                    _sync_mode = false;

   rpc_server_config                                       _tmp_rpc_config;

   bts::net::node_ptr                                      _p2p_node = nullptr;
   bool                                                    _simulated_network = false;

   bts_gntp_notifier_ptr                                   _notifier;
   fc::future<void>                                        _blocks_too_old_monitor_done;

   const unsigned                                          _blockchain_synopsis_size_limit;

   fc::future<void>                                        _client_done;

   uint32_t                                                _debug_last_wait_block = 0;
   uint32_t                                                _debug_stop_before_block_num = 0;

   void wallet_http_callback( const string& url, const ledger_entry& e );
   boost::signals2::scoped_connection   _http_callback_signal_connection;
   //-------------------------------------------------- JSON-RPC Method Implementations
   // include all of the method overrides generated by the bts_api_generator
   // this file just contains a bunch of lines that look like:
   // virtual void some_method(const string& some_arg) override;
#include <bts/rpc_stubs/common_api_overrides.ipp> //include auto-generated RPC API declarations
};

} } } // namespace bts::client::detail

