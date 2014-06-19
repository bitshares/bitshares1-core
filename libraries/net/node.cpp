#include <sstream>
#include <iomanip>
#include <deque>
#include <unordered_set>
#include <list>
#include <iostream>
#include <boost/tuple/tuple.hpp>
#include <boost/circular_buffer.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>

#include <fc/thread/thread.hpp>
#include <fc/thread/future.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/network/rate_limiting.hpp>

#include <bts/net/node.hpp>
#include <bts/net/peer_database.hpp>
#include <bts/net/message_oriented_connection.hpp>
#include <bts/net/stcp_socket.hpp>
#include <bts/net/config.hpp>
#include <bts/client/messages.hpp>

#include <bts/utilities/git_revision.hpp>
#include <fc/git_revision.hpp>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

namespace bts { namespace net {
  namespace detail
  {
    class peer_connection : public message_oriented_connection_delegate,
                            public std::enable_shared_from_this<peer_connection>
    {
    public:
#if 0
      enum connection_state
      {
        disconnected, 
        just_connected, // if inbound
        secure_connection_established, // ecdh complete
        hello_sent,
        connection_accepted_sent,
        connection_rejected_sent, 
        connected,
        connection_accepted,
        connection_rejected,
        connection_closing_message_sent,
        connection_closed
      };
#endif
      enum class our_connection_state
      {
        disconnected, 
        just_connected, // if in this state, we have sent a hello_message
        connection_accepted, // remote side has sent us a connection_accepted, we're operating normally with them
        connection_rejected // remote side has sent us a connection_rejected, we may be exchanging address with them or may just be waiting for them to close
      };
      enum class their_connection_state
      {
        disconnected,
        just_connected, // we have not yet received a hello_message
        connection_accepted, // we have sent them a connection_accepted
        connection_rejected // we have sent them a connection_rejected
      };
    private:
      node_impl&                     _node;
      fc::optional<fc::ip::endpoint> _remote_endpoint;
      message_oriented_connection    _message_connection;
    public:
      fc::time_point connection_initiation_time;
      fc::time_point connection_closed_time;
      peer_connection_direction direction;
      //connection_state state;
      firewalled_state is_firewalled;
      fc::microseconds latency;

      our_connection_state our_state;
      bool they_have_requested_close;
      their_connection_state their_state;
      bool we_have_requested_close;

      fc::time_point get_connection_time()const { return _message_connection.get_connection_time(); }

      /// data about the peer node
      /// @{
      node_id_t        node_id;
      uint32_t         core_protocol_version;
      std::string      user_agent;
      fc::optional<std::string> bitshares_git_revision_sha;
      fc::optional<fc::time_point_sec> bitshares_git_revision_unix_timestamp;
      fc::optional<std::string> fc_git_revision_sha;
      fc::optional<fc::time_point_sec> fc_git_revision_unix_timestamp;
      fc::optional<std::string> platform;

      // for inbound connections, these fields record what the peer sent us in
      // its hello message.  For outbound, they record what we sent the peer 
      // in our hello message
      fc::ip::address inbound_address;
      uint16_t inbound_port;
      uint16_t outbound_port;
      /// @}

      typedef std::unordered_map<item_id, fc::time_point> item_to_time_map_type;

      /// blockchain synchronization state data
      /// @{
      std::deque<item_hash_t> ids_of_items_to_get; /// id of items in the blockchain that this peer has told us about
      uint32_t number_of_unfetched_item_ids; /// number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their ids
      bool peer_needs_sync_items_from_us;
      bool we_need_sync_items_from_peer;
      fc::optional<boost::tuple<item_id, fc::time_point> > item_ids_requested_from_peer; /// we check this to detect a timed-out request and in busy()
      item_to_time_map_type sync_items_requested_from_peer; /// ids of blocks we've requested from this peer during sync.  fetch from another peer if this peer disconnects
      uint32_t initial_block_number; /// the block number they were on when we first connected to the peer
      uint32_t last_block_number_delegate_has_seen; /// the number of the last block this peer has told us about that the delegate knows (ids_of_items_to_get[0] should be the id of block [this value + 1])
      item_hash_t last_block_delegate_has_seen; /// the hash of the last block  this peer has told us about that the peer knows 
      /// @}

      /// non-synchronization state data
      /// @{
      std::unordered_set<item_id> inventory_peer_advertised_to_us;
      std::unordered_set<item_id> inventory_advertised_to_peer; /// TODO: make this a map to the time/block# we advertised it so we can expire items off of the list

      item_to_time_map_type items_requested_from_peer;  /// items we've requested from this peer during normal operation.  fetch from another peer if this peer disconnects
      /// @}
    public:
      peer_connection(node_impl& n) : 
        _node(n),
        _message_connection(this),
        direction(peer_connection_direction::unknown),
        is_firewalled(firewalled_state::unknown),
        our_state(our_connection_state::disconnected),
        they_have_requested_close(false),
        their_state(their_connection_state::disconnected),
        we_have_requested_close(false),
        number_of_unfetched_item_ids(0),
        peer_needs_sync_items_from_us(true),
        we_need_sync_items_from_peer(true),
        last_block_number_delegate_has_seen(0)
      {}
      ~peer_connection() {}

      fc::tcp_socket& get_socket();
      void accept_connection();
      void connect_to(const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint = fc::optional<fc::ip::endpoint>());

      void on_message(message_oriented_connection* originating_connection, const message& received_message) override;
      void on_connection_closed(message_oriented_connection* originating_connection) override;

      void send_message(const message& message_to_send);
      void close_connection();

      uint64_t get_total_bytes_sent() const;
      uint64_t get_total_bytes_received() const;

      fc::time_point get_last_message_sent_time() const;
      fc::time_point get_last_message_received_time() const;

      fc::optional<fc::ip::endpoint> get_remote_endpoint();
      fc::ip::endpoint get_local_endpoint();
      void set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint);

      bool busy();
      bool idle();
    private:
      void accept_connection_task();
      void connect_to_task(const fc::ip::endpoint& remote_endpoint);
    };
    typedef std::shared_ptr<peer_connection> peer_connection_ptr;


    // This specifies configuration info for the local node.  It's stored as JSON 
    // in the configuration directory (application data directory)
    struct node_configuration
    {
      fc::ip::endpoint listen_endpoint;
      bool wait_if_endpoint_is_busy;
      /**
       * Originally, our p2p code just had a 'node-id' that was a random number identifying this node
       * on the network.  This is now a private key/public key pair, where the public key is used
       * in place of the old random node-id.  The private part is unused, but might be used in
       * the future to support some notion of trusted peers.
       */
      fc::ecc::private_key private_key;
    };

 } } } // end namespace bts::net::detail

FC_REFLECT(bts::net::detail::node_configuration, (listen_endpoint)
                                                 (wait_if_endpoint_is_busy)
                                                 (private_key));
// not sent over the wire, just reflected for logging
FC_REFLECT_ENUM(bts::net::detail::peer_connection::our_connection_state, (disconnected)
                                                                         (just_connected)
                                                                         (connection_accepted)
                                                                         (connection_rejected))
FC_REFLECT_ENUM(bts::net::detail::peer_connection::their_connection_state, (disconnected)
                                                                           (just_connected)
                                                                           (connection_accepted)
                                                                           (connection_rejected))

namespace bts { namespace net { 
  namespace detail 
  {


/////////////////////////////////////////////////////////////////////////////////////////////////////////
    class blockchain_tied_message_cache 
    {
    private:
      static const uint32_t cache_duration_in_blocks = 2;

      struct message_hash_index{};
      struct message_contents_hash_index{};
      struct block_clock_index{};
      struct message_info
      {
        message_hash_type message_hash;
        message           message_body;
        uint32_t          block_clock_when_received;

        // for network performance stats
        message_propagation_data propagation_data;
        fc::uint160_t     message_contents_hash; // hash of whatever the message contains (if it's a transaction, this is the transaction id, if it's a block, it's the block_id)

        message_info(const message_hash_type& message_hash,
                     const message&           message_body,
                     uint32_t                 block_clock_when_received,
                     const message_propagation_data& propagation_data,
                     fc::uint160_t            message_contents_hash) :
          message_hash(message_hash),
          message_body(message_body),
          block_clock_when_received(block_clock_when_received),
          propagation_data(propagation_data),
          message_contents_hash(message_contents_hash)
        {}
      };
      typedef boost::multi_index_container<message_info, 
                                          boost::multi_index::indexed_by<boost::multi_index::ordered_unique<boost::multi_index::tag<message_hash_index>, 
                                                                                                            boost::multi_index::member<message_info, message_hash_type, &message_info::message_hash> >,
                                                                         boost::multi_index::ordered_non_unique<boost::multi_index::tag<message_contents_hash_index>, 
                                                                                                                boost::multi_index::member<message_info, fc::uint160_t, &message_info::message_contents_hash> >,
                                                                         boost::multi_index::ordered_non_unique<boost::multi_index::tag<block_clock_index>, 
                                                                                                                boost::multi_index::member<message_info, uint32_t, &message_info::block_clock_when_received> > > > message_cache_container;
      message_cache_container _message_cache;

      uint32_t block_clock;
    public:
      blockchain_tied_message_cache() :
        block_clock(0)
      {}
      void block_accepted();
      void cache_message(const message& message_to_cache, const message_hash_type& hash_of_message_to_cache, 
                         const message_propagation_data& propagation_data, const fc::uint160_t& message_content_hash);
      message get_message(const message_hash_type& hash_of_message_to_lookup);
      message_propagation_data get_message_propagation_data(const fc::uint160_t& hash_of_message_contents_to_lookup) const;
      size_t size() const { return _message_cache.size(); }
    };

    void blockchain_tied_message_cache::block_accepted()
    {
      ++block_clock;
      if (block_clock > cache_duration_in_blocks)
        _message_cache.get<block_clock_index>().erase(_message_cache.get<block_clock_index>().begin(), 
                                                      _message_cache.get<block_clock_index>().lower_bound(block_clock - cache_duration_in_blocks));
    }

    void blockchain_tied_message_cache::cache_message(const message& message_to_cache, 
                                                      const message_hash_type& hash_of_message_to_cache, 
                                                      const message_propagation_data& propagation_data, 
                                                      const fc::uint160_t& message_content_hash)
    {
      _message_cache.insert(message_info(hash_of_message_to_cache, 
                                         message_to_cache, 
                                         block_clock, 
                                         propagation_data, 
                                         message_content_hash));
    }

    message blockchain_tied_message_cache::get_message(const message_hash_type& hash_of_message_to_lookup)
    {
      message_cache_container::index<message_hash_index>::type::const_iterator iter = _message_cache.get<message_hash_index>().find(hash_of_message_to_lookup);
      if (iter != _message_cache.get<message_hash_index>().end())
        return iter->message_body;
      FC_THROW_EXCEPTION( fc::key_not_found_exception, "Requested message not in cache");
    }
    message_propagation_data blockchain_tied_message_cache::get_message_propagation_data(const fc::uint160_t& hash_of_message_contents_to_lookup) const
    {
      if (hash_of_message_contents_to_lookup != fc::uint160_t())
      {
        message_cache_container::index<message_contents_hash_index>::type::const_iterator iter = _message_cache.get<message_contents_hash_index>().find(hash_of_message_contents_to_lookup);
        if (iter != _message_cache.get<message_contents_hash_index>().end())
          return iter->propagation_data;
      }
      FC_THROW_EXCEPTION( fc::key_not_found_exception, "Requested message not in cache");
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////


    class node_impl
    {
    public:
      node_delegate*       _delegate;
      fc::sha256           _chain_id;

#define NODE_CONFIGURATION_FILENAME      "node_config.json"
#define POTENTIAL_PEER_DATABASE_FILENAME "peers.leveldb"
      fc::path             _node_configuration_directory;
      node_configuration   _node_configuration;

      /// stores the endpoint we're listening on.  This will be the same as 
      // _node_configuration.listen_endpoint, unless that endpoint was already
      // in use.
      fc::ip::endpoint     _actual_listening_endpoint;

      /// used by the task that manages connecting to peers
      // @{
      peer_database          _potential_peer_db;
      std::list<potential_peer_record> _add_once_node_list; /// list of peers we want to connect to as soon as possible
      fc::promise<void>::ptr _retrigger_connect_loop_promise;
      bool                   _potential_peer_database_updated;
      fc::future<void>       _p2p_network_connect_loop_done;
      // @}

      /// used by the task that fetches sync items during synchronization
      // @{
      fc::promise<void>::ptr _retrigger_fetch_sync_items_loop_promise;
      bool                   _sync_items_to_fetch_updated;
      fc::future<void>       _fetch_sync_items_loop_done;
      typedef std::unordered_map<bts::blockchain::block_id_type, fc::time_point> active_sync_requests_map;
      active_sync_requests_map              _active_sync_requests; /// list of sync blocks we've asked for from peers but have not yet received
      std::list<bts::client::block_message> _received_sync_items; /// list of sync blocks we've received, but can't yet process because we are still missing blocks that come earlier in the chain
      // @}

      /// used by the task that fetches items during normal operation
      // @{
      fc::promise<void>::ptr _retrigger_fetch_item_loop_promise;
      bool                   _items_to_fetch_updated;
      fc::future<void>       _fetch_item_loop_done;

      typedef boost::multi_index_container<item_id, 
                                           boost::multi_index::indexed_by<boost::multi_index::sequenced<>,
                                                                          boost::multi_index::hashed_unique<boost::multi_index::identity<item_id>, std::hash<item_id> > >
                                           > items_to_fetch_set_type;
      items_to_fetch_set_type _items_to_fetch; /// list of items we know another peer has and we want
      // @}

      /// used by the task that advertises inventory during normal operation
      // @{
      fc::promise<void>::ptr _retrigger_advertise_inventory_loop_promise;
      fc::future<void>       _advertise_inventory_loop_done;
      std::unordered_set<item_id> _new_inventory; /// list of items we have received but not yet advertised to our peers
      // @}

      fc::future<void>       _terminate_inactive_connections_loop_done;

      std::string          _user_agent_string;
      node_id_t            _node_id;

      /** if we have less than `_desired_number_of_connections`, we will try to connect with more nodes */
      uint32_t             _desired_number_of_connections;
      /** if we have _maximum_number_of_connections or more, we will refuse any inbound connections */
      uint32_t             _maximum_number_of_connections;
      /** retry connections to peers that have failed or rejected us this often, in seconds */
      uint32_t              _peer_connection_retry_timeout;
      /** how many seconds of inactivity are permitted before disconnecting a peer */
      uint32_t              _peer_inactivity_timeout;

      fc::tcp_server       _tcp_server;
      fc::future<void>     _accept_loop_complete;

      /** Stores all connections which have not yet finished key exchange or are still sending initial handshaking messages
       * back and forth (not yet ready to initiate syncing) */
      std::unordered_set<peer_connection_ptr>                     _handshaking_connections;
      /** stores fully established connections we're either syncing with or in normal operation with */
      std::unordered_set<peer_connection_ptr>                     _active_connections;
      /** stores connections we've closed, but are still waiting for the remote end to close before we delete them */
      std::unordered_set<peer_connection_ptr>                     _closing_connections;
      
      boost::circular_buffer<item_hash_t> _most_recent_blocks_accepted; // the /n/ most recent blocks we've accepted (currently tuned to the max number of connections)
      uint32_t _sync_item_type;
      uint32_t _total_number_of_unfetched_items; /// the number of items we still need to fetch while syncing

      blockchain_tied_message_cache _message_cache; /// cache message we have received and might be required to provide to other peers via inventory requests

      fc::rate_limiting_group _rate_limiter;

#ifdef ENABLE_P2P_DEBUGGING_API
      std::set<node_id_t> _allowed_peers;
#endif // ENABLE_P2P_DEBUGGING_API

      node_impl();
      ~node_impl();

      void save_node_configuration();

      void p2p_network_connect_loop();
      void trigger_p2p_network_connect_loop();

      bool have_already_received_sync_item(const item_hash_t& item_hash);
      void request_sync_item_from_peer(const peer_connection_ptr& peer, const item_hash_t& item_to_request);
      void fetch_sync_items_loop();
      void trigger_fetch_sync_items_loop();

      void fetch_items_loop();
      void trigger_fetch_items_loop();

      void advertise_inventory_loop();
      void trigger_advertise_inventory_loop();

      void terminate_inactive_connections_loop();

      bool is_accepting_new_connections();
      bool is_wanting_new_connections();
      uint32_t get_number_of_connections();

      bool is_already_connected_to_id(const node_id_t node_id);
      bool merge_address_info_with_potential_peer_database(const std::vector<address_info> addresses);
      void display_current_connections();
      uint32_t calculate_unsynced_block_count_from_all_peers();
      std::vector<item_hash_t> create_blockchain_synopsis_for_peer(const peer_connection* peer);
      void fetch_next_batch_of_item_ids_from_peer(peer_connection* peer, bool reset_fork_tracking_data_for_peer = false);

      fc::variant_object generate_hello_user_data();
      void parse_hello_user_data_for_peer(peer_connection* originating_peer, const fc::variant_object& user_data);

      void on_message(peer_connection* originating_peer, const message& received_message);
      void on_hello_message(peer_connection* originating_peer, const hello_message& hello_message_received);
      void on_connection_accepted_message(peer_connection* originating_peer, const connection_accepted_message& connection_accepted_message_received);
      void on_connection_rejected_message(peer_connection* originating_peer, const connection_rejected_message& connection_rejected_message_received);
      void on_address_request_message(peer_connection* originating_peer, const address_request_message& address_request_message_received);
      void on_address_message(peer_connection* originating_peer, const address_message& address_message_received);
      void on_fetch_blockchain_item_ids_message(peer_connection* originating_peer, const fetch_blockchain_item_ids_message& fetch_blockchain_item_ids_message_received);
      void on_blockchain_item_ids_inventory_message(peer_connection* originating_peer, const blockchain_item_ids_inventory_message& blockchain_item_ids_inventory_message_received);
      void on_fetch_items_message(peer_connection* originating_peer, const fetch_items_message& fetch_items_message_received);
      void on_item_not_available_message(peer_connection* originating_peer, const item_not_available_message& item_not_available_message_received);
      void on_item_ids_inventory_message(peer_connection* originating_peer, const item_ids_inventory_message& item_ids_inventory_message_received);
      void on_closing_connection_message(peer_connection* originating_peer, const closing_connection_message& closing_connection_message_received);
      void on_current_time_request_message(peer_connection* originating_peer, const current_time_request_message& current_time_request_message_received);
      void on_current_time_reply_message(peer_connection* originating_peer, const current_time_reply_message& current_time_reply_message_received);
      void on_check_firewall_message(peer_connection* originating_peer, const check_firewall_message& check_firewall_message_received);
      void on_check_firewall_reply_message(peer_connection* originating_peer, const check_firewall_reply_message& check_firewall_reply_message_received);
      void on_connection_closed(peer_connection* originating_peer);

      void process_backlog_of_sync_blocks();
      void process_block_during_sync(peer_connection* originating_peer, const bts::client::block_message& block_message, const message_hash_type& message_hash);
      void process_block_during_normal_operation(peer_connection* originating_peer, const bts::client::block_message& block_message, const message_hash_type& message_hash);
      void process_block_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash);
  
      void process_ordinary_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash);

      void start_synchronizing();
      void start_synchronizing_with_peer(const peer_connection_ptr& peer);

      void new_peer_just_added(const peer_connection_ptr& peer); /// called after a peer finishes handshaking, kicks off syncing

      void close();

      void accept_connection_task(peer_connection_ptr new_peer);
      void accept_loop();
      void send_hello_message(const peer_connection_ptr& peer);
      void connect_to_task(peer_connection_ptr new_peer, const fc::ip::endpoint& remote_endpoint);
      peer_connection_ptr get_connection_to_endpoint(const fc::ip::endpoint& remote_endpoint);
      bool is_connection_to_endpoint_in_progress(const fc::ip::endpoint& remote_endpoint);

      void dump_node_status();
      
      void disconnect_from_peer(peer_connection* originating_peer,
                                const std::string& reason_for_disconnect,
                                bool caused_by_error = false,
                                const fc::oexception& additional_data = fc::oexception());

      // methods implementing node's public interface
      void set_node_delegate(node_delegate* del);
      void load_configuration(const fc::path& configuration_directory);
      void listen_to_p2p_network();
      void connect_to_p2p_network();
      void add_node(const fc::ip::endpoint& ep);
      void connect_to(const fc::ip::endpoint& ep);
      void listen_on_endpoint(const fc::ip::endpoint& ep);
      void listen_on_port(uint16_t port, bool wait_if_not_available);
      fc::ip::endpoint get_actual_listening_endpoint() const;
      std::vector<peer_status> get_connected_peers() const;
      uint32_t get_connection_count() const;
      void broadcast(const message& item_to_broadcast, const message_propagation_data& propagation_data);
      void broadcast(const message& item_to_broadcast);
      void sync_from(const item_id&);
      bool is_connected() const;
      void set_advanced_node_parameters(const fc::variant_object& params);
      fc::variant_object get_advanced_node_parameters();
      message_propagation_data get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id);
      message_propagation_data get_block_propagation_data(const bts::blockchain::block_id_type& block_id);
      node_id_t get_node_id() const;
      void set_allowed_peers(const std::vector<node_id_t>& allowed_peers);
      void clear_peer_database();
      void set_total_bandwidth_limit(uint32_t upload_bytes_per_second, uint32_t download_bytes_per_second);
      fc::variant_object network_get_info() const;
    }; // end class node_impl

    fc::tcp_socket& peer_connection::get_socket()
    {
      return _message_connection.get_socket();
    }

    void peer_connection::accept_connection()
    {
      try
      {
        assert(our_state == our_connection_state::disconnected &&
               their_state == their_connection_state::disconnected);
        direction = peer_connection_direction::inbound;
        _message_connection.accept();           // perform key exchange
        _remote_endpoint = _message_connection.get_socket().remote_endpoint();
        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog("established inbound connection from ${remote_endpoint}, sending hello", ("remote_endpoint", _message_connection.get_socket().remote_endpoint()));
      }
      catch (const fc::exception& e)
      {
        wlog("error accepting connection ${e}", ("e", e.to_detail_string()));
        throw;
      }
    }

    void peer_connection::connect_to(const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint)
    {
      try
      {
        assert(our_state == our_connection_state::disconnected && their_state == their_connection_state::disconnected);
        direction = peer_connection_direction::outbound;

        _remote_endpoint = remote_endpoint;
        if (local_endpoint)
        {
          // the caller wants us to bind the local side of this socket to a specific ip/port
          // This depends on the ip/port being unused, and on being able to set the 
          // SO_REUSEADDR/SO_REUSEPORT flags, and either of these might fail, so we need to 
          // detect if this fails.
          try
          {
            _message_connection.bind(*local_endpoint);
          }
          catch (const fc::exception& except)
          {
            wlog("Failed to bind to desired local endpoint ${endpoint}, will connect using an OS-selected endpoint: ${except}", ("endpoint", *local_endpoint)("except", except));
          }
        }
        _message_connection.connect_to(remote_endpoint);
        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog("established outbound connection to ${remote_endpoint}", ("remote_endpoint", remote_endpoint));
      }
      catch (fc::exception& e)
      {
        elog("fatal: error connecting to peer ${remote_endpoint}: ${e}", ("remote_endpoint", remote_endpoint)("e", e.to_detail_string()));
        throw;
      }
    } // connect_to()

    void peer_connection::on_message(message_oriented_connection* originating_connection, const message& received_message)
    {
      _node.on_message(this, received_message);
    }

    void peer_connection::on_connection_closed(message_oriented_connection* originating_connection)
    {
      _node.on_connection_closed(this);
    }

    void peer_connection::send_message(const message& message_to_send)
    {
      _message_connection.send_message(message_to_send);
    }

    void peer_connection::close_connection()
    {
      _message_connection.close_connection();
    }

    uint64_t peer_connection::get_total_bytes_sent() const
    {
      return _message_connection.get_total_bytes_sent();
    }

    uint64_t peer_connection::get_total_bytes_received() const
    {
      return _message_connection.get_total_bytes_received();
    }

    fc::time_point peer_connection::get_last_message_sent_time() const
    {
      return _message_connection.get_last_message_sent_time();
    }

    fc::time_point peer_connection::get_last_message_received_time() const
    {
      return _message_connection.get_last_message_received_time();
    }

    fc::optional<fc::ip::endpoint> peer_connection::get_remote_endpoint()
    {
      return _remote_endpoint;
    }
    fc::ip::endpoint peer_connection::get_local_endpoint()
    {
      return _message_connection.get_socket().local_endpoint();
    }

    void peer_connection::set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint)
    {
      _remote_endpoint = new_remote_endpoint;
    }

    bool peer_connection::busy() 
    { 
      return !items_requested_from_peer.empty() || !sync_items_requested_from_peer.empty() || item_ids_requested_from_peer;
    }

    bool peer_connection::idle()
    {
      return !busy();
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    node_impl::node_impl() : 
      _delegate(nullptr),
      _user_agent_string("bts::net::node"),
      _desired_number_of_connections(8),
      _maximum_number_of_connections(12),
      _peer_connection_retry_timeout(BTS_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME),
      _peer_inactivity_timeout( BTS_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT),
      _most_recent_blocks_accepted(_maximum_number_of_connections),
      _total_number_of_unfetched_items(0),
      _rate_limiter(0, 0)
    {
    }

    node_impl::~node_impl()
    {
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*active_peer->get_remote_endpoint());
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);
      }

      try
      {
        close();
      }
      catch (const fc::exception& e)
      {
        wlog("unexpected exception on close ${e}", ("e", e.to_detail_string()));
      }
    }

    void node_impl::save_node_configuration()
    {
      if (fc::exists(_node_configuration_directory))
      {
        fc::path configuration_file_name(_node_configuration_directory / NODE_CONFIGURATION_FILENAME);
        try
        {
          fc::json::save_to_file(_node_configuration, configuration_file_name);
        }
        catch (fc::exception& except)
        {
          elog("error writing node configuration to file ${filename}: ${error}", 
               ("filename", configuration_file_name)("error", except.to_detail_string()));
        }
      }
    }

    void node_impl::p2p_network_connect_loop()
    {
      for (;;)
      {
        try 
        {
          dlog("Starting an iteration of p2p_network_connect_loop().");
          display_current_connections();
         
          // add-once peers bypass our checks on the maximum/desired number of connections (but they will still be counted against the totals once they're connected)
          if (!_add_once_node_list.empty())
          {
            std::list<potential_peer_record> add_once_node_list;
            add_once_node_list.swap(_add_once_node_list);
            dlog("Processing \"add once\" node list containing ${count} peers:", ("count", add_once_node_list.size()));
            for (const potential_peer_record& add_once_peer : add_once_node_list)
            {
              dlog("    ${peer}", ("peer", add_once_peer.endpoint));
            }
            for (const potential_peer_record& add_once_peer : add_once_node_list)
            {
              // see if we have an existing connection to that peer.  If we do, disconnect them and
              // then try to connect the next time through the loop
              peer_connection_ptr existing_connection_ptr = get_connection_to_endpoint(add_once_peer.endpoint);
              if (!existing_connection_ptr)
                connect_to(add_once_peer.endpoint);
            }
            dlog("Done processing \"add once\" node list");
          }
         
          while (is_wanting_new_connections())
          {
            bool initiated_connection_this_pass = false;
            _potential_peer_database_updated = false;
         
            for ( peer_database::iterator iter = _potential_peer_db.begin();
                 iter != _potential_peer_db.end() && is_wanting_new_connections();
                 ++iter)
            {
             /**
              *
              *
              */
              auto delay_until_retry = fc::seconds((iter->number_of_failed_connection_attempts + 1) * _peer_connection_retry_timeout);

              if (!is_connection_to_endpoint_in_progress(iter->endpoint) &&
                  ((iter->last_connection_disposition != last_connection_failed && 
                    iter->last_connection_disposition != last_connection_rejected &&
                    iter->last_connection_disposition != last_connection_handshaking_failed) ||
                    (fc::time_point::now() - iter->last_connection_attempt_time) > delay_until_retry ))
              {
                connect_to(iter->endpoint);
                initiated_connection_this_pass = true;
              }
            }
         
            if (!initiated_connection_this_pass && !_potential_peer_database_updated)
              break;
          }
         
          display_current_connections();
         
         
         
          // if we broke out of the while loop, that means either we have connected to enough nodes, or
          // we don't have any good candidates to connect to right now.
          try
          {
            _retrigger_connect_loop_promise = fc::promise<void>::ptr(new fc::promise<void>());
            if (is_wanting_new_connections() || !_add_once_node_list.empty())
            {
              if (is_wanting_new_connections())
                dlog("Still want to connect to more nodes, but I don't have any good candidates.  Trying again in 15 seconds");
              else
                dlog("I still have some \"add once\" nodes to connect to.  Trying again in 15 seconds");
              _retrigger_connect_loop_promise->wait_until(fc::time_point::now() + fc::seconds(BTS_PEER_DATABASE_RETRY_DELAY));
            }
            else
            {
              dlog("I don't need any more connections, waiting forever until something changes");
              _retrigger_connect_loop_promise->wait();
            }
          }
          catch (fc::timeout_exception&) //intentionally not logged
          {
          }  // catch
        } 
        catch ( const fc::exception& e )
        {
          elog( "${e}", ("e",e.to_detail_string() ) );
        }
      }// for( ;; )
    } 

    void node_impl::trigger_p2p_network_connect_loop()
    {
      dlog("Triggering connect loop now");
      _potential_peer_database_updated = true;
      if (_retrigger_connect_loop_promise)
        _retrigger_connect_loop_promise->set_value();
    }

    bool node_impl::have_already_received_sync_item(const item_hash_t& item_hash)
    {
      return std::find_if(_received_sync_items.begin(), _received_sync_items.end(), 
                          [&item_hash](const bts::client::block_message& message) { return message.block_id == item_hash; }) != _received_sync_items.end();
    }

    void node_impl::request_sync_item_from_peer(const peer_connection_ptr& peer, const item_hash_t& item_to_request)
    {
      dlog("requesting item ${item_hash} from peer ${endpoint}", ("item_hash", item_to_request)("endpoint", peer->get_remote_endpoint()));
      item_id item_id_to_request(bts::client::block_message_type, item_to_request);
      _active_sync_requests.insert(active_sync_requests_map::value_type(item_to_request, fc::time_point::now()));
      peer->sync_items_requested_from_peer.insert(peer_connection::item_to_time_map_type::value_type(item_id_to_request, fc::time_point::now()));
      std::vector<item_hash_t> items_to_fetch;
      peer->send_message(fetch_items_message(item_id_to_request.item_type, std::vector<item_hash_t>{item_id_to_request.item_hash}));
    }

    void node_impl::fetch_sync_items_loop()
    {
      for (;;)
      {
        _sync_items_to_fetch_updated = false;
        dlog("beginning another iteration of the sync items loop");

        std::map<peer_connection_ptr, item_hash_t> sync_item_requests_to_send;
        std::set<item_hash_t> sync_items_to_request;

        // for each idle peer that we're syncing with
        for (const peer_connection_ptr& peer : _active_connections)
        {
          if (peer->we_need_sync_items_from_peer && 
              sync_item_requests_to_send.find(peer) == sync_item_requests_to_send.end() && // if we've already scheduled a request for this peer, don't consider scheduling another
              peer->idle())
          {
            // loop through the items it has that we don't yet have on our blockchain
            for (unsigned i = 0; i < peer->ids_of_items_to_get.size(); ++i)
            {
              item_hash_t item_to_potentially_request = peer->ids_of_items_to_get[i];
              // if we don't already have this item in our temporary storage and we haven't requested from another syncing peer
              if (!have_already_received_sync_item(item_to_potentially_request) && // already got it, but for some reson it's still in our list of items to fetch
                  sync_items_to_request.find(item_to_potentially_request) == sync_items_to_request.end() &&  // we have already decided to request it from another peer during this iteration
                  _active_sync_requests.find(item_to_potentially_request) == _active_sync_requests.end()) // we've requested it in a previous iteration and we're still waiting for it to arrive
              {
                // then schedule a request from this peer
                sync_item_requests_to_send[peer] = item_to_potentially_request;
                sync_items_to_request.insert(item_to_potentially_request);
                break;
              }
            }
          }
        }

        // make all the requests we scheduled in the loop above
        for (auto sync_item_request : sync_item_requests_to_send)
          request_sync_item_from_peer(sync_item_request.first, sync_item_request.second);

        if (!_sync_items_to_fetch_updated)
        {
          dlog("no sync items to fetch right now, going to sleep");
          _retrigger_fetch_sync_items_loop_promise = fc::promise<void>::ptr(new fc::promise<void>());
          _retrigger_fetch_sync_items_loop_promise->wait();
          _retrigger_fetch_sync_items_loop_promise.reset();
        }
      }
    }

    void node_impl::trigger_fetch_sync_items_loop()
    {
      dlog("Triggering fetch sync items loop now");
      _sync_items_to_fetch_updated = true;
      if (_retrigger_fetch_sync_items_loop_promise)
        _retrigger_fetch_sync_items_loop_promise->set_value();
    }

    void node_impl::fetch_items_loop()
    {
      for (;;)
      {
        _items_to_fetch_updated = false;
        dlog("beginning an iteration of fetch items (${count} items to fetch)", ("count", _items_to_fetch.size()));

        for (auto iter = _items_to_fetch.begin(); iter != _items_to_fetch.end(); )
        {
          bool item_fetched = false;
          for (const peer_connection_ptr& peer : _active_connections)
          {
            if (peer->idle() &&
                peer->inventory_peer_advertised_to_us.find(*iter) != peer->inventory_peer_advertised_to_us.end())
            {
              dlog("requesting item ${hash} from peer ${endpoint}", ("hash", iter->item_hash)("endpoint", peer->get_remote_endpoint()));
              peer->items_requested_from_peer.insert(peer_connection::item_to_time_map_type::value_type(*iter, fc::time_point::now()));
              item_id item_id_to_fetch = *iter;
              iter = _items_to_fetch.erase(iter);
              item_fetched = true;
              peer->send_message(fetch_items_message(item_id_to_fetch.item_type, 
                                                     std::vector<item_hash_t>{item_id_to_fetch.item_hash}));
              break;
            }
#ifndef NDEBUG
            else if (peer->inventory_peer_advertised_to_us.find(*iter) != peer->inventory_peer_advertised_to_us.end())
            {
              dlog("would request item ${hash} from peer ${endpoint}, but it is busy", ("hash", iter->item_hash)("endpoint", peer->get_remote_endpoint()));
            }
#endif
          }
          if (!item_fetched)
            ++iter;
        }

        if (!_items_to_fetch_updated)
        {
          _retrigger_fetch_item_loop_promise = fc::promise<void>::ptr(new fc::promise<void>());
          _retrigger_fetch_item_loop_promise->wait();
          _retrigger_fetch_item_loop_promise.reset();
        }
      }
    }

    void node_impl::trigger_fetch_items_loop()
    {
      _items_to_fetch_updated = true;
      if (_retrigger_fetch_item_loop_promise)
        _retrigger_fetch_item_loop_promise->set_value();
    }

    void node_impl::advertise_inventory_loop()
    {
      for (;;)
      {
        dlog("beginning an iteration of advertise inventory");
        // swap inventory into local variable, clearing the node's copy
        std::unordered_set<item_id> inventory_to_advertise;
        inventory_to_advertise.swap(_new_inventory);

        // process all inventory to advertise and construct the inventory messages we'll send
        // first, then send them all in a batch (to avoid any fiber interruption points while
        // we're computing the messages)
        std::list<std::pair<peer_connection_ptr, item_ids_inventory_message> > inventory_messages_to_send;

        for (const peer_connection_ptr& peer : _active_connections)
        {
          // only advertise to peers who are in sync with us
          if (!peer->peer_needs_sync_items_from_us)
          {
            std::map<uint32_t, std::vector<item_hash_t> > items_to_advertise_by_type;
            // don't send the peer anything we've already advertised to it
            // or anything it has advertised to us
            // group the items we need to send by type, because we'll need to send one inventory message per type
            unsigned total_items_to_send_to_this_peer = 0;
            for (const item_id& item_to_advertise : inventory_to_advertise)
              if (peer->inventory_advertised_to_peer.find(item_to_advertise) == peer->inventory_advertised_to_peer.end() &&
                  peer->inventory_peer_advertised_to_us.find(item_to_advertise) == peer->inventory_peer_advertised_to_us.end())
              {
                items_to_advertise_by_type[item_to_advertise.item_type].push_back(item_to_advertise.item_hash);
                peer->inventory_advertised_to_peer.insert(item_to_advertise);
                ++total_items_to_send_to_this_peer;
                dlog("advertising item ${id} to peer ${endpoint}", ("id", item_to_advertise.item_hash)("endpoint", peer->get_remote_endpoint()));
              }
              dlog("advertising ${count} new item(s) of ${types} type(s) to peer ${endpoint}", 
                   ("count", total_items_to_send_to_this_peer)("types", items_to_advertise_by_type.size())("endpoint", peer->get_remote_endpoint()));
            for (auto items_group : items_to_advertise_by_type)
              inventory_messages_to_send.push_back(std::make_pair(peer, item_ids_inventory_message(items_group.first, items_group.second)));
          }
        }

        for (auto iter = inventory_messages_to_send.begin(); iter != inventory_messages_to_send.end(); ++iter)
          iter->first->send_message(iter->second);

        if (_new_inventory.empty())
        {
          _retrigger_advertise_inventory_loop_promise = fc::promise<void>::ptr(new fc::promise<void>());
          _retrigger_advertise_inventory_loop_promise->wait();
          _retrigger_advertise_inventory_loop_promise.reset();
        }
      }
    }

    void node_impl::trigger_advertise_inventory_loop()
    {
      if (_retrigger_advertise_inventory_loop_promise)
        _retrigger_advertise_inventory_loop_promise->set_value();
    }

    void node_impl::terminate_inactive_connections_loop()
    {
      for (;;)
      {
        std::list<peer_connection_ptr> peers_to_disconnect_gently;
        std::list<peer_connection_ptr> peers_to_disconnect_forcibly;

        // Disconnect peers that haven't sent us any data recently
        // These numbers are just guesses and we need to think through how this works better.
        // If we and our peers get disconnected from the rest of the network, we will not 
        // receive any blocks or transactions from the rest of the world, and that will 
        // probably make us disconnect from our peers even though we have working connections to
        // them (but they won't have sent us anything since they aren't getting blocks either).
        // This might not be so bad because it could make us initiate more connections and
        // reconnect with the rest of the network, or it might just futher isolate us.
      
        uint32_t handshaking_timeout = _peer_inactivity_timeout;
        fc::time_point handshaking_disconnect_threshold = fc::time_point::now() - fc::seconds(handshaking_timeout);
        for (const peer_connection_ptr handshaking_peer : _handshaking_connections)
          if (handshaking_peer->connection_initiation_time < handshaking_disconnect_threshold &&
              handshaking_peer->get_last_message_received_time() < handshaking_disconnect_threshold &&
              handshaking_peer->get_last_message_sent_time() < handshaking_disconnect_threshold)
          {
            wlog("Forcibly disconnecting from handshaking peer ${peer} due to inactivity of at least ${timeout} seconds", 
                 ("peer", handshaking_peer->get_remote_endpoint())("timeout", handshaking_timeout));            
            peers_to_disconnect_forcibly.push_back(handshaking_peer);
          }

        uint32_t active_timeout = _peer_inactivity_timeout * 20;
        fc::time_point active_disconnect_threshold = fc::time_point::now() - fc::seconds(active_timeout);
        for (const peer_connection_ptr& active_peer : _active_connections)
          if (active_peer->connection_initiation_time < active_disconnect_threshold &&
              active_peer->get_last_message_received_time() < active_disconnect_threshold &&
              active_peer->get_last_message_sent_time() < active_disconnect_threshold)
          {
            wlog("Closing connection with peer ${peer} due to inactivity of at least ${timeout} seconds", 
                 ("peer", active_peer->get_remote_endpoint())("timeout", active_timeout));
            peers_to_disconnect_gently.push_back(active_peer);
          }

        fc::time_point closing_disconnect_threshold = fc::time_point::now() - fc::seconds(BTS_NET_PEER_DISCONNECT_TIMEOUT);
        for (const peer_connection_ptr& closing_peer : _closing_connections)
          if (closing_peer->connection_closed_time < closing_disconnect_threshold)
          {
            // we asked this peer to close their connectoin to us at least BTS_NET_PEER_DISCONNECT_TIMEOUT
            // seconds ago, but they haven't done it yet.  Terminate the connection now
            wlog("Forcibly disconnecting peer ${peer} who failed to close their conneciton in a timely manner", 
                 ("peer", closing_peer->get_remote_endpoint()));
            peers_to_disconnect_forcibly.push_back(closing_peer);
          }

        for (const peer_connection_ptr& peer : peers_to_disconnect_gently)
        {
          fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Disconnecting due to inactivity", 
                                                      ("last_message_received_seconds_ago", (peer->get_last_message_received_time() - fc::time_point::now()).count() / fc::seconds(1).count())
                                                      ("last_message_sent_seconds_ago", (peer->get_last_message_sent_time() - fc::time_point::now()).count() / fc::seconds(1).count())
                                                      ("inactivity_timeout", _active_connections.find(peer) != _active_connections.end() ? _peer_inactivity_timeout * 10 : _peer_inactivity_timeout)));
          disconnect_from_peer(peer.get(), "Disconnecting due to inactivity", false, detailed_error);
        }

        for (const peer_connection_ptr& peer : peers_to_disconnect_forcibly)
          peer->close_connection();

        fc::usleep(fc::seconds(BTS_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT/2));
      }
    }

    bool node_impl::is_accepting_new_connections()
    {
      return get_number_of_connections() <= _maximum_number_of_connections;
    }

    bool node_impl::is_wanting_new_connections()
    {
      return get_number_of_connections() < _desired_number_of_connections;
    }

    uint32_t node_impl::get_number_of_connections()
    {
      return _handshaking_connections.size() + _active_connections.size();
    }

    bool node_impl::is_already_connected_to_id(const node_id_t node_id)
    {
      if (_node_id == node_id)
      {
        dlog("is_already_connected_to_id returning true because the peer is us");
        return true;
      }
      for (const peer_connection_ptr active_peer : _active_connections)
        if (active_peer->node_id == node_id)
        {
          dlog("is_already_connected_to_id returning true because the peer is already in our active list");
          return true;
        }
      for (const peer_connection_ptr handshaking_peer : _handshaking_connections)
        if (handshaking_peer->node_id == node_id)
        {
          dlog("is_already_connected_to_id returning true because the peer is already in our handshaking list");
          return true;
        }
      return false;
    }

    // merge addresses received from a peer into our database
    bool node_impl::merge_address_info_with_potential_peer_database(const std::vector<address_info> addresses)
    {
      bool new_information_received = false;
      for (const address_info& address : addresses)
      {
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(address.remote_endpoint);
        if (address.last_seen_time > updated_peer_record.last_seen_time)
          new_information_received = true;
        updated_peer_record.last_seen_time = std::max(address.last_seen_time, updated_peer_record.last_seen_time);
        _potential_peer_db.update_entry(updated_peer_record);
      }
      return new_information_received;
    }

    void node_impl::display_current_connections()
    {
      dlog("Currently have ${current} of [${desired}/${max}] connections", 
            ("current", get_number_of_connections())
            ("desired", _desired_number_of_connections)
            ("max", _maximum_number_of_connections));
      dlog("   my id is ${id}", ("id", _node_id));

      for (const peer_connection_ptr& active_connection : _active_connections)
      {
        dlog("        active: ${endpoint} with ${id}   [${direction}]", ("endpoint",active_connection->get_remote_endpoint())("id",active_connection->node_id)("direction",active_connection->direction));
      }
      for (const peer_connection_ptr& handshaking_connection : _handshaking_connections)
      {
        dlog("   handshaking: ${endpoint} with ${id}  [${direction}]", ("endpoint",handshaking_connection->get_remote_endpoint())("id",handshaking_connection->node_id)("direction",handshaking_connection->direction));
      }
    }

    void node_impl::on_message(peer_connection* originating_peer, const message& received_message)
    {
      message_hash_type message_hash = received_message.id();
      dlog("handling message ${type} ${hash} size ${size} from peer ${endpoint}", 
           ("type", bts::net::core_message_type_enum(received_message.msg_type))("hash", message_hash)("size", received_message.size)("endpoint", originating_peer->get_remote_endpoint()));
      switch (received_message.msg_type)
      {
      case core_message_type_enum::hello_message_type:
        on_hello_message(originating_peer, received_message.as<hello_message>());
        break;
      case core_message_type_enum::connection_accepted_message_type:
        on_connection_accepted_message(originating_peer, received_message.as<connection_accepted_message>());
        break;
      case core_message_type_enum::connection_rejected_message_type:
        on_connection_rejected_message(originating_peer, received_message.as<connection_rejected_message>());
        break;
      case core_message_type_enum::address_request_message_type:
        on_address_request_message(originating_peer, received_message.as<address_request_message>());
        break;
      case core_message_type_enum::address_message_type:
        on_address_message(originating_peer, received_message.as<address_message>());
        break;
      case core_message_type_enum::fetch_blockchain_item_ids_message_type:
        on_fetch_blockchain_item_ids_message(originating_peer, received_message.as<fetch_blockchain_item_ids_message>());
        break;
      case core_message_type_enum::blockchain_item_ids_inventory_message_type:
        on_blockchain_item_ids_inventory_message(originating_peer, received_message.as<blockchain_item_ids_inventory_message>());
        break;
      case core_message_type_enum::fetch_items_message_type:
        on_fetch_items_message(originating_peer, received_message.as<fetch_items_message>());
        break;
      case core_message_type_enum::item_not_available_message_type:
        on_item_not_available_message(originating_peer, received_message.as<item_not_available_message>());
        break;
      case core_message_type_enum::item_ids_inventory_message_type:
        on_item_ids_inventory_message(originating_peer, received_message.as<item_ids_inventory_message>());
        break;
      case core_message_type_enum::closing_connection_message_type:
        on_closing_connection_message(originating_peer, received_message.as<closing_connection_message>());
        break;
      case bts::client::message_type_enum::block_message_type:
        process_block_message(originating_peer, received_message, message_hash);
        break;
      case core_message_type_enum::current_time_request_message_type:
        on_current_time_request_message(originating_peer, received_message.as<current_time_request_message>());
        break;
      case core_message_type_enum::current_time_reply_message_type:
        on_current_time_reply_message(originating_peer, received_message.as<current_time_reply_message>());
        break;
      case core_message_type_enum::check_firewall_message_type:
        on_check_firewall_message(originating_peer, received_message.as<check_firewall_message>());
        break;
      case core_message_type_enum::check_firewall_reply_message_type:
        on_check_firewall_reply_message(originating_peer, received_message.as<check_firewall_reply_message>());
        break;

      default:
        process_ordinary_message(originating_peer, received_message, message_hash);
        break;
      }
    }


    fc::variant_object node_impl::generate_hello_user_data()
    {
      // for the time being, shoehorn a bunch of properties into the user_data variant object, 
      // which lets us add and remove fields without changing the protocol.  Once we
      // settle on what we really want in there, we'll likely promote them to first
      // class fields in the hello message
      fc::mutable_variant_object user_data;
      user_data["bitshares_git_revision_sha"] = bts::utilities::git_revision_sha;
      user_data["bitshares_git_revision_unix_timestamp"] = bts::utilities::git_revision_unix_timestamp;
      user_data["fc_git_revision_sha"] = fc::git_revision_sha;
      user_data["fc_git_revision_unix_timestamp"] = fc::git_revision_unix_timestamp;
#if defined(__APPLE__)
      user_data["platform"] = "osx";
#elif defined(__linux__)
      user_data["platform"] = "linux";
#elif defined(_MSC_VER)
      user_data["platform"] = "win32";
#else 
      user_data["platform"] = "other";
#endif
      return user_data;
    }
    void node_impl::parse_hello_user_data_for_peer(peer_connection* originating_peer, const fc::variant_object& user_data)
    {
      // try to parse data out of the user_agent string
      if (user_data.contains("bitshares_git_revision_sha"))
        originating_peer->bitshares_git_revision_sha = user_data["bitshares_git_revision_sha"].as_string();
      if (user_data.contains("bitshares_git_revision_unix_timestamp"))
        originating_peer->bitshares_git_revision_unix_timestamp = fc::time_point_sec(user_data["bitshares_git_revision_unix_timestamp"].as<uint32_t>());
      if (user_data.contains("fc_git_revision_sha"))
        originating_peer->fc_git_revision_sha = user_data["fc_git_revision_sha"].as_string();
      if (user_data.contains("fc_git_revision_unix_timestamp"))
        originating_peer->fc_git_revision_unix_timestamp = fc::time_point_sec(user_data["fc_git_revision_unix_timestamp"].as<uint32_t>());
      if (user_data.contains("platform"))
        originating_peer->platform = user_data["platform"].as_string();
    }

    void node_impl::on_hello_message(peer_connection* originating_peer, const hello_message& hello_message_received)
    {
      // this check must come before we fill in peer data below
      bool already_connected_to_this_peer = is_already_connected_to_id(hello_message_received.node_id);

      // store off the data provided in the hello message
      originating_peer->user_agent = hello_message_received.user_agent;
      originating_peer->node_id = hello_message_received.node_id;
      originating_peer->core_protocol_version = hello_message_received.core_protocol_version;
      originating_peer->inbound_address = hello_message_received.inbound_address;
      originating_peer->inbound_port = hello_message_received.inbound_port;
      originating_peer->outbound_port = hello_message_received.outbound_port;

      parse_hello_user_data_for_peer(originating_peer, hello_message_received.user_data);

      // now decide what to do with it
      if( originating_peer->their_state == peer_connection::their_connection_state::just_connected )
      {
        if( hello_message_received.chain_id != _chain_id )
        {
          wlog( "Recieved hello message from peer on a different chain: ${message}", ("message", hello_message_received));
          std::ostringstream rejection_message;
          rejection_message << "You're on a different chain than I am.  I'm on " << _chain_id.str() << 
                              " and you're on " << hello_message_received.chain_id.str();
          connection_rejected_message connection_rejected(_user_agent_string, core_protocol_version, 
                                                          originating_peer->get_socket().remote_endpoint(),
                                                          rejection_reason_code::different_chain,
                                                          rejection_message.str());

          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          originating_peer->send_message(message(connection_rejected));
          // for this type of message, we're immediately disconnecting this peer, instead of trying to
          // allowing her to ask us for peers (any of our peers will be on the same chain as us, so there's no
          // benefit of sharing them)
          disconnect_from_peer(originating_peer, "You are on a different chain from me");
          return;
        }
        if( already_connected_to_this_peer )
        {
          
          connection_rejected_message connection_rejected;
          if (_node_id == hello_message_received.node_id)
            connection_rejected = connection_rejected_message(_user_agent_string, core_protocol_version, 
                                                              originating_peer->get_socket().remote_endpoint(),
                                                              rejection_reason_code::connected_to_self,
                                                              "I'm connecting to myself");
          else
            connection_rejected = connection_rejected_message(_user_agent_string, core_protocol_version, 
                                                              originating_peer->get_socket().remote_endpoint(),
                                                              rejection_reason_code::already_connected,
                                                              "I'm already connected to you");
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          originating_peer->send_message(message(connection_rejected));
          dlog("Received a hello_message from peer ${peer} that I'm already connected to (with id ${id}), rejection", 
               ("peer", originating_peer->get_remote_endpoint())
               ("id", hello_message_received.node_id));
        }
#ifdef ENABLE_P2P_DEBUGGING_API
        else if( !_allowed_peers.empty() && 
                 _allowed_peers.find(originating_peer->node_id) == _allowed_peers.end() )
        {
          connection_rejected_message connection_rejected(_user_agent_string, core_protocol_version, 
                                                          originating_peer->get_socket().remote_endpoint(),
                                                          rejection_reason_code::blocked,
                                                          "you are not in my allowed_peers list");
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          originating_peer->send_message(message(connection_rejected));
          dlog("Received a hello_message from peer ${peer} who isn't in my allowed_peers list, rejection", ("peer", originating_peer->get_remote_endpoint()));
        }
#endif // ENABLE_P2P_DEBUGGING_API        
        else
        {
          // whether we're planning on accepting them as a peer or not, they seem to be a valid node,
          // so add them to our database if they're not firewalled

          // in the hello message, the peer sent us the IP address and port it thought it was connecting from.
          // If they match the IP and port we see, we assume that they're actually on the internet and they're not 
          // firewalled.
          fc::ip::endpoint peers_actual_outbound_endpoint = originating_peer->get_socket().remote_endpoint();
          if (peers_actual_outbound_endpoint.get_address() == originating_peer->inbound_address &&
              peers_actual_outbound_endpoint.port() == originating_peer->outbound_port)
          {
            if (originating_peer->inbound_port == 0)
            {
              dlog("peer does not appear to be firewalled, but they did not give an inbound port so I'm treating them as if they are.");
              originating_peer->is_firewalled = firewalled_state::firewalled;
            }
            else
            {
              // peer is not firwalled, add it to our database
              fc::ip::endpoint peers_inbound_endpoint(originating_peer->inbound_address, originating_peer->inbound_port);
              potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(peers_inbound_endpoint);
              _potential_peer_db.update_entry(updated_peer_record);
              originating_peer->is_firewalled = firewalled_state::not_firewalled;
            }
          }
          else
          {
            dlog("peer is firewalled: they think their outbound endpoing is ${reported_endpoint}, but I see it as ${actual_endpoint}",
                 ("reported_endpoint", fc::ip::endpoint(originating_peer->inbound_address, originating_peer->outbound_port))
                 ("actual_endpoint", peers_actual_outbound_endpoint));
            originating_peer->is_firewalled = firewalled_state::firewalled;
          }

          if( !is_accepting_new_connections() )
          {
            connection_rejected_message connection_rejected(_user_agent_string, core_protocol_version, 
                                                            originating_peer->get_socket().remote_endpoint(),
                                                            rejection_reason_code::not_accepting_connections,
                                                            "not accepting any more incoming connections");
            originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
            originating_peer->send_message(message(connection_rejected));
            dlog("Received a hello_message from peer ${peer}, but I'm not accepting any more connections, rejection", 
                 ("peer", originating_peer->get_remote_endpoint()));
          }
          else
          {
            originating_peer->their_state = peer_connection::their_connection_state::connection_accepted;
            originating_peer->send_message(message(connection_accepted_message()));
            dlog("Received a hello_message from peer ${peer}, sending reply to accept connection", ("peer", originating_peer->get_remote_endpoint()));
          }
        }
      }
      else
      {
        // we can wind up here if we've connected to ourself, and the source and
        // destination endpoints are the same, causing messages we send out
        // to arrive back on the initiating socket instead of the receiving
        // socket.  If we did a complete job of enumerating local addresses,
        // we could avoid directly connecting to ourselves, or at least detect
        // immediately when we did it and disconnect.

        // The only way I know of that we'd get an unexpected hello that we 
        //  can't really guard against is if we do a simulatenous open, we 
        // probably need to think through that case.  We're not attempting that
        // yet, though, so it's ok to just disconnect here.
        wlog("unexpected hello_message from peer, disconnecting");
        disconnect_from_peer(originating_peer, "Received a unexpected hello_message");
      }
    }

    void node_impl::on_connection_accepted_message(peer_connection* originating_peer, const connection_accepted_message& connection_accepted_message_received)
    {
#if 0
      bool already_connected_to_this_peer = is_already_connected_to_id(hello_reply_message_received.node_id);

      // store off the data provided in the hello message
      originating_peer->user_agent = hello_reply_message_received.user_agent;
      originating_peer->node_id = hello_reply_message_received.node_id;
      originating_peer->core_protocol_version = hello_reply_message_received.core_protocol_version;
      parse_hello_user_data_for_peer(originating_peer, hello_reply_message_received.user_data);

      // report whether this peer will think we're behind a firewall
      if (originating_peer->inbound_port != 0)
      {
        // if we sent inbound_port = 0 we were telling them that we're firewalled and don't accept incoming connections
        if (originating_peer->inbound_address == hello_reply_message_received.remote_endpoint.get_address() &&
            originating_peer->outbound_port == hello_reply_message_received.remote_endpoint.port())
          dlog("peer ${peer} does not think we're behind a firewall", ("peer", originating_peer->get_remote_endpoint()));
        else
          dlog("peer ${peer} thinks we're firewalled (we think we were connecting from ${we_saw}, they saw ${they_saw})",
               ("peer", originating_peer->get_remote_endpoint())
               ("we_saw", fc::ip::endpoint(originating_peer->inbound_address, originating_peer->outbound_port))
               ("they_saw", hello_reply_message_received.remote_endpoint));
      }
      if (originating_peer->state == peer_connection::hello_sent && 
          originating_peer->direction == peer_connection_direction::outbound)
      {
        if (already_connected_to_this_peer)
        {
          dlog("Established a connection with peer ${peer}, but I'm already connected to it.  Closing the connection", 
               ("peer", originating_peer->get_remote_endpoint()));
          disconnect_from_peer(originating_peer, "I'm already connected to you");
        }
#ifdef ENABLE_P2P_DEBUGGING_API
        else if (!_allowed_peers.empty() && 
                 _allowed_peers.find(originating_peer->node_id) == _allowed_peers.end())
        {
          dlog("Established a connection with peer ${peer}, but it's not in my _accepted_peers list.  Closing the connection", 
               ("peer", originating_peer->get_remote_endpoint()));
          disconnect_from_peer(originating_peer, "You're not in my accepted_peers list");
        }
#endif // ENABLE_P2P_DEBUGGING_API        
        else
        {
          dlog("Received a reply to my \"hello\" from ${peer}, connection is accepted", ("peer", originating_peer->get_remote_endpoint()));
          dlog("Remote server sees my connection as ${endpoint}", ("endpoint", hello_reply_message_received.remote_endpoint));
          originating_peer->state = peer_connection::connected;
          originating_peer->send_message(address_request_message());
        }
      }
      else
        FC_THROW("unexpected hello_reply_message from peer");
#endif
      dlog("Received a connection_accepted in response to my \"hello\" from ${peer}", ("peer", originating_peer->get_remote_endpoint()));
      originating_peer->our_state = peer_connection::our_connection_state::connection_accepted;
      originating_peer->send_message(address_request_message());
    }

    void node_impl::on_connection_rejected_message(peer_connection* originating_peer, const connection_rejected_message& connection_rejected_message_received)
    {
      if (originating_peer->our_state == peer_connection::our_connection_state::just_connected)
      {
        ilog("Received a rejection from ${peer} in response to my \"hello\", reason: \"${reason}\"", 
             ("peer", originating_peer->get_remote_endpoint())
             ("reason", connection_rejected_message_received.reason_string));

        if (connection_rejected_message_received.reason_code == rejection_reason_code::connected_to_self)
        {
          _potential_peer_db.erase(originating_peer->get_socket().remote_endpoint());
        }
        else
        {
          // update our database to record that we were rejected so we won't try to connect again for a while
          // this only happens on connections we originate, so we should already know that peer is not firewalled
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(originating_peer->get_socket().remote_endpoint());
          updated_peer_record.last_connection_disposition = last_connection_rejected;
          updated_peer_record.last_connection_attempt_time = fc::time_point::now();
          _potential_peer_db.update_entry(updated_peer_record);
        }

        originating_peer->our_state = peer_connection::our_connection_state::connection_rejected;
        originating_peer->send_message(address_request_message());
      }
      else
        FC_THROW("unexpected connection_rejected_message from peer");
    }

    void node_impl::on_address_request_message(peer_connection* originating_peer, const address_request_message& address_request_message_received)
    {
      dlog("Received an address request message");

      address_message reply;
      reply.addresses.reserve(_active_connections.size() );
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*active_peer->get_remote_endpoint());
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);

        address_info info_for_peer(*active_peer->get_remote_endpoint(), 
                                   fc::time_point::now(),
                                   active_peer->latency,
                                   active_peer->node_id,
                                   active_peer->direction,
                                   active_peer->is_firewalled);
        reply.addresses.push_back(std::move(info_for_peer));
      }

      //for (const potential_peer_record& record : _potential_peer_db)
      originating_peer->send_message(reply);
    }

    void node_impl::on_address_message(peer_connection* originating_peer, const address_message& address_message_received)
    {
      dlog("Received an address message containing ${size} addresses", ("size", address_message_received.addresses.size()));
      for (const address_info& address : address_message_received.addresses)
      {
        dlog("    ${endpoint} last seen ${time}", ("endpoint", address.remote_endpoint)("time", address.last_seen_time));
      }
      bool new_information_received = merge_address_info_with_potential_peer_database(address_message_received.addresses);
      if (new_information_received)
        trigger_p2p_network_connect_loop();

      if (originating_peer->our_state == peer_connection::our_connection_state::connection_rejected)      
        disconnect_from_peer(originating_peer, "You rejected my connection request (hello message) so I'm disconnecting");
      else if (originating_peer->their_state == peer_connection::their_connection_state::connection_rejected)
        disconnect_from_peer(originating_peer, "I rejected your connection request (hello message) so I'm disconnecting");
      else
      {
        if (originating_peer->is_firewalled == firewalled_state::not_firewalled)
        {
          // mark the connection as successful in the database
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*originating_peer->get_remote_endpoint());
          updated_peer_record.last_connection_disposition = last_connection_succeeded;
          _potential_peer_db.update_entry(updated_peer_record);
        }

        _active_connections.insert(originating_peer->shared_from_this());
        _handshaking_connections.erase(originating_peer->shared_from_this());
        new_peer_just_added(originating_peer->shared_from_this());
      }
    }

    void node_impl::on_fetch_blockchain_item_ids_message(peer_connection* originating_peer, 
                                                         const fetch_blockchain_item_ids_message& fetch_blockchain_item_ids_message_received)
    {
      item_id peers_last_item_seen;
      if (!fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty())
        peers_last_item_seen = item_id(fetch_blockchain_item_ids_message_received.item_type,
                                       fetch_blockchain_item_ids_message_received.blockchain_synopsis.back());
      dlog("sync: received a request for item ids after ${last_item_seen} from peer ${peer_endpoint} (full request: ${synopsis})", 
           ("last_item_seen", peers_last_item_seen)
           ("peer_endpoint", originating_peer->get_remote_endpoint())
           ("synopsis", fetch_blockchain_item_ids_message_received.blockchain_synopsis));

      blockchain_item_ids_inventory_message reply_message;
      reply_message.item_hashes_available = _delegate->get_item_ids(fetch_blockchain_item_ids_message_received.item_type,
                                                                    fetch_blockchain_item_ids_message_received.blockchain_synopsis,
                                                                    reply_message.total_remaining_item_count);
      reply_message.item_type = fetch_blockchain_item_ids_message_received.item_type;

      // if our client doesn't have any items after the item the peer requested, it will send back
      // a list containing the last item the peer requested
      if (reply_message.item_hashes_available.empty()) 
        originating_peer->peer_needs_sync_items_from_us = false; /* I have no items in my blockchain */ 
      else if (!fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
               reply_message.item_hashes_available.size() == 1 &&
               reply_message.item_hashes_available.back() == fetch_blockchain_item_ids_message_received.blockchain_synopsis.back())
        /* the last item in the peer's list matches the last item in our list */
        originating_peer->peer_needs_sync_items_from_us = false;
      else
        originating_peer->peer_needs_sync_items_from_us = true;

      if (!originating_peer->peer_needs_sync_items_from_us)
      {
        dlog("sync: peer is already in sync with us");
        // if we thought we had all the items this peer had, but now it turns out that we don't 
        // have the last item it requested to send from, 
        // we need to kick off another round of synchronization
        if (!originating_peer->we_need_sync_items_from_peer &&
            !fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
            !_delegate->has_item(peers_last_item_seen))
        {
          dlog("sync: restarting sync with peer ${peer}", ("peer", originating_peer->get_remote_endpoint()));
          start_synchronizing_with_peer(originating_peer->shared_from_this());
        }
      }
      else
      {
        dlog("sync: peer is out of sync, sending peer ${count} items ids: ${item_ids}", ("count", reply_message.item_hashes_available.size())("item_ids", reply_message.item_hashes_available));
        if (!originating_peer->we_need_sync_items_from_peer &&
            !fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
            !_delegate->has_item(peers_last_item_seen))
        {
          dlog("sync: restarting sync with peer ${peer}", ("peer", originating_peer->get_remote_endpoint()));
          start_synchronizing_with_peer(originating_peer->shared_from_this());
        }
      }
      originating_peer->send_message(reply_message);

      if (originating_peer->direction == peer_connection_direction::inbound &&
          _handshaking_connections.find(originating_peer->shared_from_this()) != _handshaking_connections.end())
      {
        // handshaking is done, move the connection to fully active status and start synchronizing
        dlog("peer ${endpoint} which was handshaking with us has started synchronizing with us, start syncing with it", 
             ("endpoint", originating_peer->get_remote_endpoint()));
        
        if (originating_peer->is_firewalled == firewalled_state::not_firewalled)
        {
          // mark the connection as successful in the database
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*originating_peer->get_remote_endpoint());
          updated_peer_record.last_connection_disposition = last_connection_succeeded;
          _potential_peer_db.update_entry(updated_peer_record);
        }

        // transition it to our active list
        _active_connections.insert(originating_peer->shared_from_this());
        _handshaking_connections.erase(originating_peer->shared_from_this());
        new_peer_just_added(originating_peer->shared_from_this());
      }
    }

    uint32_t node_impl::calculate_unsynced_block_count_from_all_peers()
    {
      uint32_t max_number_of_unfetched_items = 0;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        uint32_t this_peer_number_of_unfetched_items = peer->ids_of_items_to_get.size() + peer->number_of_unfetched_item_ids;
        max_number_of_unfetched_items = std::max(max_number_of_unfetched_items,
                                                 this_peer_number_of_unfetched_items);
      }
      return max_number_of_unfetched_items;
    }

    // get a blockchain synopsis that makes sense to send to the given peer.
    // If the peer isn't yet syncing with us, this is just a synopsis of our active blockchain
    // If the peer is syncing with us, it is a synopsis of our active blockchain plus the 
    //    blocks the peer has already told us it has
    std::vector<item_hash_t> node_impl::create_blockchain_synopsis_for_peer(const peer_connection* peer)
    {
      item_hash_t reference_point;
      uint32_t number_of_blocks_after_reference_point = 0;

      reference_point = peer->last_block_delegate_has_seen;
      number_of_blocks_after_reference_point = peer->ids_of_items_to_get.size();

      std::vector<item_hash_t> synopsis = _delegate->get_blockchain_synopsis(_sync_item_type, reference_point, number_of_blocks_after_reference_point);
      
      // if we passed in a reference point, we believe it is one the client has already accepted and should
      // be able to generate a synopsis based on it
      if (reference_point != item_hash_t() && synopsis.empty())
      {
        synopsis = _delegate->get_blockchain_synopsis(_sync_item_type, reference_point, number_of_blocks_after_reference_point);
      }
      assert(reference_point == item_hash_t() || !synopsis.empty());

      if (number_of_blocks_after_reference_point)
      {
        // then the synopsis is incomplete, add the missing elements from ids_of_items_to_get
        uint32_t true_high_block_num = peer->last_block_number_delegate_has_seen + number_of_blocks_after_reference_point;
        uint32_t low_block_num = 1;
        do
        {
          if (low_block_num > peer->last_block_number_delegate_has_seen)
            synopsis.push_back(peer->ids_of_items_to_get[low_block_num - peer->last_block_number_delegate_has_seen - 1]);
          low_block_num += ((true_high_block_num - low_block_num + 2) / 2);
        }
        while (low_block_num <= true_high_block_num);
        assert(synopsis.back() == peer->ids_of_items_to_get.back());
      }
      return synopsis;
    }

    void node_impl::fetch_next_batch_of_item_ids_from_peer(peer_connection* peer, bool reset_fork_tracking_data_for_peer /* = false */)
    {
      if (reset_fork_tracking_data_for_peer)
      {
        peer->last_block_delegate_has_seen = item_hash_t();
        peer->last_block_number_delegate_has_seen = 0;
      }

      std::vector<item_hash_t> blockchain_synopsis = create_blockchain_synopsis_for_peer(peer);
      item_hash_t last_item_seen = blockchain_synopsis.empty() ? item_hash_t() : blockchain_synopsis.back();
      dlog("sync: sending a request for the next items after ${last_item_seen} to peer ${peer}, (full request is ${blockchain_synopsis})", 
           ("last_item_seen", last_item_seen)
           ("peer", peer->get_remote_endpoint())
           ("blockchain_synopsis", blockchain_synopsis));
      peer->item_ids_requested_from_peer = boost::make_tuple(item_id(_sync_item_type, last_item_seen), fc::time_point::now());
      //std::vector<item_hash_t> blockchain_synopsis = _delegate->get_blockchain_synopsis(last_item_id_seen.item_type, last_item_id_seen.item_hash);
      //assert(last_item_id_seen.item_hash == item_hash_t() || last_item_id_seen.item_hash == blockchain_synopsis.back());
      //ilog("actual last item from blockchain synopsis is ${last_item_seen_for_real}", ("last_item_seen_for_real", blockchain_synopsis.empty() ? item_hash_t() : blockchain_synopsis.back()));
      peer->send_message(fetch_blockchain_item_ids_message(_sync_item_type, blockchain_synopsis));
    }

    void node_impl::on_blockchain_item_ids_inventory_message(peer_connection* originating_peer,
                                                             const blockchain_item_ids_inventory_message& blockchain_item_ids_inventory_message_received)
    {
      // ignore unless we asked for the data
      if (originating_peer->item_ids_requested_from_peer)
      {
#if 0
        assert(originating_peer->item_ids_requested_from_peer->get<0>().item_hash == item_hash_t() ||
               blockchain_item_ids_inventory_message_received.item_hashes_available.empty() ||
               blockchain_item_ids_inventory_message_received.item_hashes_available.front() == originating_peer->item_ids_requested_from_peer->get<0>().item_hash);
#endif
        originating_peer->item_ids_requested_from_peer.reset();

        dlog("sync: received a list of ${count} available items from ${peer_endpoint}", 
             ("count", blockchain_item_ids_inventory_message_received.item_hashes_available.size())
             ("peer_endpoint", originating_peer->get_remote_endpoint()));
        for (const item_hash_t& item_hash : blockchain_item_ids_inventory_message_received.item_hashes_available)
        {
          dlog("sync:     ${hash}", ("hash", item_hash));
        }

        // if the peer doesn't have any items after the one we asked for
        if (blockchain_item_ids_inventory_message_received.total_remaining_item_count == 0 &&
            (blockchain_item_ids_inventory_message_received.item_hashes_available.empty() || // there are no items in the peer's blockchain.  this should only happen if our blockchain was empty when we requested, might want to verify that.
             (blockchain_item_ids_inventory_message_received.item_hashes_available.size() == 1 && 
              _delegate->has_item(item_id(blockchain_item_ids_inventory_message_received.item_type,
                                          blockchain_item_ids_inventory_message_received.item_hashes_available.front())))) && // we've already seen the last item in the peer's blockchain
            originating_peer->ids_of_items_to_get.empty() &&
            originating_peer->number_of_unfetched_item_ids == 0) // <-- is the last check necessary?
        {
          dlog("sync: peer said we're up-to-date, entering normal operation with this peer");
          originating_peer->we_need_sync_items_from_peer = false;

          uint32_t new_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers();
          if (new_number_of_unfetched_items == 0)
            _delegate->sync_status(blockchain_item_ids_inventory_message_received.item_type, 0);
          _total_number_of_unfetched_items = new_number_of_unfetched_items;

          return;
        }

        std::deque<item_hash_t> item_hashes_received(blockchain_item_ids_inventory_message_received.item_hashes_available.begin(), 
                                                     blockchain_item_ids_inventory_message_received.item_hashes_available.end());
        originating_peer->number_of_unfetched_item_ids = blockchain_item_ids_inventory_message_received.total_remaining_item_count;
        // flush any items this peer sent us that we've already received and processed from another peer
        if (!item_hashes_received.empty() &&
            originating_peer->ids_of_items_to_get.empty())
        {
          bool is_first_item_for_other_peer = false;
          for (const peer_connection_ptr& peer : _active_connections)
            if (peer != originating_peer->shared_from_this() &&
                !peer->ids_of_items_to_get.empty() &&
                peer->ids_of_items_to_get.front() == blockchain_item_ids_inventory_message_received.item_hashes_available.front())
            {
              dlog("The item ${newitem} is the first item for peer ${peer}",
                   ("newitem", blockchain_item_ids_inventory_message_received.item_hashes_available.front())
                   ("peer", peer->get_remote_endpoint()));
              is_first_item_for_other_peer = true; 
              break;
            }
          dlog("is_first_item_for_other_peer: ${is_first}.  item_hashes_received.size() = ${size}",("is_first", is_first_item_for_other_peer)("size", item_hashes_received.size()));
          if (!is_first_item_for_other_peer)
          {
            while (!item_hashes_received.empty() && 
                   _delegate->has_item(item_id(blockchain_item_ids_inventory_message_received.item_type,
                                               item_hashes_received.front())))
            {
              assert(item_hashes_received.front() != item_hash_t());
              originating_peer->last_block_delegate_has_seen = item_hashes_received.front();
              ++originating_peer->last_block_number_delegate_has_seen;
              dlog("popping item because delegate has already seen it.  peer's last block the delegate has seen is now ${block_id} (${block_num})", ("block_id", originating_peer->last_block_delegate_has_seen)("block_num", originating_peer->last_block_number_delegate_has_seen));
              item_hashes_received.pop_front();
            }
            dlog("after removing all items we have already seen, item_hashes_received.size() = ${size}", ("size", item_hashes_received.size()));
          }
        } 
        else if (!item_hashes_received.empty())
        {
          // we received a list of items and we already have a list of items to fetch from this peer.
          // In the normal case, this list will immediately follow the existing list, meaning the 
          // last hash of our existing list will match the first hash of the new list.

          // In the much less likely case, we've received a partial list of items from the peer, then
          // the peer switched forks before sending us the remaining list.  In this case, the first
          // hash in the new list may not be the last hash in the existing list (it may be earlier, or
          // it may not exist at all.
          
          // In either case, pop items off the back of our existing list until we find our first 
          // item, then append our list.
          while (!originating_peer->ids_of_items_to_get.empty())
          {
            if (item_hashes_received.front() != originating_peer->ids_of_items_to_get.back())
              originating_peer->ids_of_items_to_get.pop_back();
            else
              break;
          }
          if (originating_peer->ids_of_items_to_get.empty())
          {
            // this happens when the peer has switched forks between the last inventory message and
            // this one, and there weren't any unfetched items in common
            // We don't know where in the blockchain the new front() actually falls, all we can
            // expect is that it is a block that we knew about because it should be one of the 
            // blocks we sent in the initial synopsis.
            assert(_delegate->has_item(item_id(_sync_item_type, item_hashes_received.front())));
            originating_peer->last_block_delegate_has_seen = item_hashes_received.front();
            originating_peer->last_block_number_delegate_has_seen = _delegate->get_block_number(item_hashes_received.front());
            item_hashes_received.pop_front();
          }
          else
          {
            // the common simple case: the new list extends the old.  pop off the duplicate element
            originating_peer->ids_of_items_to_get.pop_back();
          }
        }

        if (!item_hashes_received.empty() && !originating_peer->ids_of_items_to_get.empty())
          assert(item_hashes_received.front() != originating_peer->ids_of_items_to_get.back());

        // append the remaining items to the peer's list
        boost::push_back(originating_peer->ids_of_items_to_get, item_hashes_received);

        originating_peer->number_of_unfetched_item_ids = blockchain_item_ids_inventory_message_received.total_remaining_item_count;

        uint32_t new_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers();
        if (new_number_of_unfetched_items != _total_number_of_unfetched_items)
          _delegate->sync_status(blockchain_item_ids_inventory_message_received.item_type,
                                 new_number_of_unfetched_items);
        _total_number_of_unfetched_items = new_number_of_unfetched_items;
        
        if (blockchain_item_ids_inventory_message_received.total_remaining_item_count != 0)
        {
          // the peer hasn't sent us all the items it knows about.  We need to ask it for more.
          if (!originating_peer->ids_of_items_to_get.empty())
          {
            // if we have a list of sync items, keep asking for more until we get to the end of the list
            fetch_next_batch_of_item_ids_from_peer(originating_peer);
          }
          else
          {
            // If we get here, we the peer has sent us a non-empty list of items, but we have all of them
            // already.
            // Most likely, that means that some of the next blocks in sequence will be ones we have 
            // already, but we still need to ask the client for them in order.
            // (If we didn't have to handle forks, we could just jump to our last seen block,
            // but in the case where the client is telling us about a fork, we need to keep asking them
            // for sequential blocks)
            fetch_next_batch_of_item_ids_from_peer(originating_peer);
          }
        }
        else
        {
          if (!originating_peer->ids_of_items_to_get.empty())
          {
            // we now know about all of the items the peer knows about, and there are some items on the list
            // that we should try to fetch.  Kick off the fetch loop.
            trigger_fetch_sync_items_loop();
          }
          else
          {
            // If we get here, the peer has sent us a non-empty list of items, but we have already
            // received all of the items from other peers.  Send a new request to the peer to 
            // see if we're really in sync
            fetch_next_batch_of_item_ids_from_peer(originating_peer);
          }
        }
      }
      else
      {
        wlog("sync: received a list of sync items available, but I didn't ask for any!");
      }
    }

    void node_impl::on_fetch_items_message(peer_connection* originating_peer, const fetch_items_message& fetch_items_message_received)
    {
      dlog("received items request for ids ${ids} of type ${type} from peer ${endpoint}", 
           ("ids", fetch_items_message_received.items_to_fetch)
           ("type", fetch_items_message_received.item_type)
           ("endpoint", originating_peer->get_remote_endpoint()));
      
      std::list<message> reply_messages;
      for (const item_hash_t& item_hash : fetch_items_message_received.items_to_fetch)
      {
        try
        {
          message requested_message = _message_cache.get_message(item_hash);
          dlog("received item request for item ${id} from peer ${endpoint}, returning the item from my message cache",
               ("endpoint", originating_peer->get_remote_endpoint())
               ("id", requested_message.id()));
          reply_messages.push_back(requested_message);
          continue;
        }
        catch (fc::key_not_found_exception&)
        {
        }

        item_id item_to_fetch(fetch_items_message_received.item_type, item_hash);
        try
        {
          message requested_message = _delegate->get_item(item_to_fetch);
          dlog("received item request from peer ${endpoint}, returning the item from delegate with id ${id} size ${size}",
               ("id", requested_message.id())
               ("size", requested_message.size)
               ("endpoint", originating_peer->get_remote_endpoint()));
          reply_messages.push_back(requested_message);
          continue;
        }
        catch (fc::key_not_found_exception&)
        {
          reply_messages.push_back(item_not_available_message(item_to_fetch));
          dlog("received item request from peer ${endpoint} but we don't have it",
               ("endpoint", originating_peer->get_remote_endpoint()));
        }
      }
      for (const message& reply : reply_messages)
        originating_peer->send_message(reply);
    }

    void node_impl::on_item_not_available_message(peer_connection* originating_peer, const item_not_available_message& item_not_available_message_received)
    {
      auto regular_item_iter = originating_peer->items_requested_from_peer.find(item_not_available_message_received.requested_item);
      if (regular_item_iter != originating_peer->items_requested_from_peer.end())
      {
        originating_peer->items_requested_from_peer.erase(regular_item_iter);
        dlog("Peer doesn't have the requested item.");
        trigger_fetch_items_loop();
        return;
        // TODO: reschedule fetching this item from a different peer
      }

      auto sync_item_iter = originating_peer->sync_items_requested_from_peer.find(item_not_available_message_received.requested_item);
      if (sync_item_iter != originating_peer->sync_items_requested_from_peer.end())
      {
        originating_peer->sync_items_requested_from_peer.erase(sync_item_iter);
        dlog("Peer doesn't have the requested sync item.  This really shouldn't happen");
        trigger_fetch_sync_items_loop();
        return;
      }

      dlog("Peer doesn't have an item we're looking for, which is fine because we weren't looking for it");
    }

    void node_impl::on_item_ids_inventory_message(peer_connection* originating_peer, const item_ids_inventory_message& item_ids_inventory_message_received)
    {
      dlog("received inventory of ${count} items from peer ${endpoint}", 
           ("count", item_ids_inventory_message_received.item_hashes_available.size())("endpoint", originating_peer->get_remote_endpoint()));
      for (const item_hash_t& item_hash : item_ids_inventory_message_received.item_hashes_available)
      {
        item_id advertised_item_id(item_ids_inventory_message_received.item_type, item_hash);
        bool we_advertised_this_item_to_a_peer = false;
        bool we_requested_this_item_from_a_peer = false;
        for (const peer_connection_ptr peer : _active_connections)
        {
          if (peer->inventory_advertised_to_peer.find(advertised_item_id) != peer->inventory_advertised_to_peer.end())
          {
            we_advertised_this_item_to_a_peer = true;
            break;
          }
          if (peer->items_requested_from_peer.find(advertised_item_id) != peer->items_requested_from_peer.end())
            we_requested_this_item_from_a_peer = true;
        }

        // if we have already advertised it to a peer, we must have it, no need to do anything else
        if (!we_advertised_this_item_to_a_peer)
        {
          originating_peer->inventory_peer_advertised_to_us.insert(advertised_item_id);
          if (!we_requested_this_item_from_a_peer)
          {
            auto insert_result = _items_to_fetch.push_back(advertised_item_id);
            if (insert_result.second)
            {
              dlog("addinged item ${item_hash} from inventory message to our list of items to fetch",
                   ("item_hash", item_hash));
              trigger_fetch_items_loop();
            }
          }
        }
      }
      
    }

    void node_impl::on_closing_connection_message(peer_connection* originating_peer, const closing_connection_message& closing_connection_message_received)
    {
      originating_peer->they_have_requested_close = true;

      if (closing_connection_message_received.closing_due_to_error)
      {
        elog("Peer ${peer} is disconnecting us because of an error: ${msg}, exception: ${error}", 
             ("peer", originating_peer->get_remote_endpoint())
             ("msg", closing_connection_message_received.reason_for_closing)
             ("error", closing_connection_message_received.error));
        std::ostringstream message;
        message << "Peer " << fc::variant(originating_peer->get_remote_endpoint()).as_string() << 
                  " disconnected us: " << closing_connection_message_received.reason_for_closing;

        _delegate->error_encountered(message.str(), 
                                     closing_connection_message_received.error);
      }
      else
      {
        wlog("Peer ${peer} is disconnecting us because: ${msg}", 
             ("peer", originating_peer->get_remote_endpoint())
             ("msg", closing_connection_message_received.reason_for_closing));
      }
      if (originating_peer->we_have_requested_close)
        originating_peer->close_connection();
    }

    void node_impl::on_connection_closed(peer_connection* originating_peer)
    {
      peer_connection_ptr originating_peer_ptr = originating_peer->shared_from_this();
      _rate_limiter.remove_tcp_socket(&originating_peer->get_socket());
      if (_closing_connections.find(originating_peer_ptr) != _closing_connections.end())
        _closing_connections.erase(originating_peer_ptr);
      else if (_active_connections.find(originating_peer_ptr) != _active_connections.end())
      {
        _active_connections.erase(originating_peer_ptr);

        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*originating_peer_ptr->get_remote_endpoint());
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);
      }
      else if (_handshaking_connections.find(originating_peer_ptr) != _handshaking_connections.end())
        _handshaking_connections.erase(originating_peer_ptr);
      ilog("Remote peer ${endpoint} closed their connection to us", ("endpoint", originating_peer->get_remote_endpoint()));
      display_current_connections();
      trigger_p2p_network_connect_loop();
      _delegate->connection_count_changed(_active_connections.size());
    }

    void node_impl::process_backlog_of_sync_blocks()
    {
      bool block_processed_this_iteration;
      do
      {
        block_processed_this_iteration = false;
        for (auto received_block_iter = _received_sync_items.begin(); 
             received_block_iter != _received_sync_items.end(); 
             ++received_block_iter)
        {
          // find out if this block is the next block on one the active chain or one of the forks
          bool potential_first_block = false;
          for (const peer_connection_ptr& peer : _active_connections)
            if (!peer->ids_of_items_to_get.empty() &&
                peer->ids_of_items_to_get.front() == received_block_iter->block_id)
            {
              potential_first_block = true;
              break;
            }
#if 0 // just extra debugging
          if (potential_first_block)
          {
            dlog("block ${block_id} is a potential first block", ("block_id", received_block_iter->block_id));
          }
          else
          {
            dlog("block ${block_id} is not a potential first block.  potential first blocks are:", ("block_id", received_block_iter->block_id));
            for (const peer_connection_ptr& peer : _active_connections)
            {
              if (!peer->ids_of_items_to_get.empty())
                dlog(" Peer ${peer}: ${id}", ("peer", peer->get_remote_endpoint())("id", peer->ids_of_items_to_get.front()));
            }
          }
#endif

          // if it is, process it, remove it from all sync peers lists
          if (potential_first_block)
          {
            bts::client::block_message block_message_to_process = *received_block_iter;
            _received_sync_items.erase(received_block_iter);

            fc::oexception handle_message_exception;

            bool client_accepted_block = false;
            bool block_caused_fork_switch = false;
            try
            {
              dlog("sync: this block is a potential first block, passing it to the client");

              // we can get into an intersting situation near the end of synchronization.  We can be in
              // sync with one peer who is sending us the last block on the chain via a regular inventory
              // message, while at the same time still be synchronizing with a peer who is sending us the
              // block through the sync mechanism.  Further, we must request both blocks because 
              // we don't know they're the same (for the peer in normal operation, it has only told us the
              // message id, for the peer in the sync case we only known the block_id).
              if (std::find(_most_recent_blocks_accepted.begin(), _most_recent_blocks_accepted.end(),
                            block_message_to_process.block_id) == _most_recent_blocks_accepted.end())
              {
                block_caused_fork_switch = _delegate->handle_message(block_message_to_process, true);
                _most_recent_blocks_accepted.push_back(block_message_to_process.block_id);
              }
              else
                dlog("Already received and accepted this block (presumably through normal inventory mechanism), treating it as accepted");

              client_accepted_block = true;
            }
            catch (fc::exception& e)
            {
              wlog("sync: client rejected sync block sent by peer");
              handle_message_exception = e;
            }

            if (client_accepted_block)
            {
              --_total_number_of_unfetched_items;
              block_processed_this_iteration = true;
              dlog("sync: client accpted the block, we now have only ${count} items left to fetch before we're in sync", ("count", _total_number_of_unfetched_items));
              std::set<peer_connection_ptr> peers_with_newly_empty_item_lists;
              std::set<peer_connection_ptr> peers_we_need_to_sync_to;
              for (const peer_connection_ptr& peer : _active_connections)
              {
                if (peer->ids_of_items_to_get.empty())
                {
                  dlog("Cannot pop first element off peer ${peer}'s list, its list is empty", ("peer", peer->get_remote_endpoint()));
                  // we don't know for sure that this peer has the item we just received.
                  // If peer is still syncing to us, we know they will ask us for
                  // sync item ids at least one more time and we'll notify them about
                  // the item then, so there's no need to do anything.  If we still need items
                  // from them, we'll be asking them for more items at some point, and
                  // that will clue them in that they are out of sync.  If we're fully in sync 
                  // we need to kick off another round of synchronization with them so they can 
                  // find out about the new item.
                  if (!peer->peer_needs_sync_items_from_us && !peer->we_need_sync_items_from_peer)
                  {
                    dlog("We will be restarting synchronization with peer ${peer}", ("peer", peer->get_remote_endpoint()));
                    peers_we_need_to_sync_to.insert(peer);
                  }
                }
                else
                {
                  if (peer->ids_of_items_to_get.front() == block_message_to_process.block_id)
                  {
                    peer->last_block_delegate_has_seen = block_message_to_process.block_id;
                    ++peer->last_block_number_delegate_has_seen;

                    peer->ids_of_items_to_get.pop_front();
                    dlog("Popped item from front of ${endpoint}'s sync list, new list length is ${len}", ("endpoint", peer->get_remote_endpoint())("len", peer->ids_of_items_to_get.size()));

                    // if we just received the last item in our list from this peer, we will want to 
                    // send another request to find out if we are in sync, but we can't do this yet
                    // (we don't want to allow a fiber swap in the middle of popping items off the list)
                    if (peer->ids_of_items_to_get.empty() && peer->number_of_unfetched_item_ids == 0)
                      peers_with_newly_empty_item_lists.insert(peer);

                    // in this case, we know the peer was offering us this exact item, no need to 
                    // try to inform them of its existence
                  }
                  else
                  {
                    // the peer's list of sync items is nonempty, and its first item doesn't match
                    // the one we just accepted.  This happens when we're synchronizing with 
                    // peers on two different forks.
                    dlog("Cannot pop first element off peer ${peer}'s list, its first is ${hash}", ("peer", peer->get_remote_endpoint())("hash", peer->ids_of_items_to_get.front()));
                  }
                }
              }
              for (const peer_connection_ptr& peer : peers_with_newly_empty_item_lists)
                fetch_next_batch_of_item_ids_from_peer(peer.get());

              for (const peer_connection_ptr& peer : peers_we_need_to_sync_to)
                start_synchronizing_with_peer(peer);
            }
            else
            {
              // invalid message received
              std::list<peer_connection_ptr> peers_to_disconnect;
              for (const peer_connection_ptr& peer : _active_connections)
                if (!peer->ids_of_items_to_get.empty() &&
                    peer->ids_of_items_to_get.front() == block_message_to_process.block_id)
                  peers_to_disconnect.push_back(peer);
              for (const peer_connection_ptr& peer : peers_to_disconnect)
              {
                wlog("disconnecting client ${endpoint} because it offered us the rejected block", ("endpoint", peer->get_remote_endpoint()));
                disconnect_from_peer(peer.get(), "You offered us a block that we reject as invalid", true, handle_message_exception);
              }
              break;
            }              

            break; // start iterating _received_sync_items from the beginning
          } // end if potential_first_block
        } // end for each block in _received_sync_items
      } while (block_processed_this_iteration);
      dlog("Currently backlog is ${count} blocks", ("count", _received_sync_items.size()));
    }

    void node_impl::process_block_during_sync(peer_connection* originating_peer, const bts::client::block_message& block_message_to_process, const message_hash_type& message_hash)
    {
      dlog("received a sync block from peer ${endpoint}", ("endpoint", originating_peer->get_remote_endpoint()));

      // add it to the front of _received_sync_items, then process _received_sync_items to try to 
      // pass as many messages as possible to the client.
      _received_sync_items.push_front(block_message_to_process);
      process_backlog_of_sync_blocks();

      // we should be ready to request another block now
      trigger_fetch_sync_items_loop();
    }

    void node_impl::process_block_during_normal_operation(peer_connection* originating_peer, const bts::client::block_message& block_message_to_process, const message_hash_type& message_hash)
    {
      fc::time_point message_receive_time = fc::time_point::now();

      //dump_node_status();
      dlog("received a block from peer ${endpoint}, passing it to client", ("endpoint", originating_peer->get_remote_endpoint()));
      trigger_fetch_items_loop();

      try
      {
        // we can get into an intersting situation near the end of synchronization.  We can be in
        // sync with one peer who is sending us the last block on the chain via a regular inventory
        // message, while at the same time still be synchronizing with a peer who is sending us the
        // block through the sync mechanism.  Further, we must request both blocks because 
        // we don't know they're the same (for the peer in normal operation, it has only told us the
        // message id, for the peer in the sync case we only known the block_id).
        fc::time_point message_validated_time;
        bool block_caused_fork_switch = false;
        if (std::find(_most_recent_blocks_accepted.begin(), _most_recent_blocks_accepted.end(), 
                      block_message_to_process.block_id) == _most_recent_blocks_accepted.end())
        {
          block_caused_fork_switch = _delegate->handle_message(block_message_to_process, false);
          message_validated_time = fc::time_point::now();
          _most_recent_blocks_accepted.push_back(block_message_to_process.block_id);
        }
        else
          dlog("Already received and accepted this block (presumably through sync mechanism), treating it as accepted");

        dlog("client validated the block, advertising it to other peers");

        for (const peer_connection_ptr& peer : _active_connections)
        {
          item_id block_message_item_id(bts::client::message_type_enum::block_message_type, message_hash);
          auto iter = peer->inventory_peer_advertised_to_us.find(block_message_item_id);
          if (iter != peer->inventory_peer_advertised_to_us.end())
          {
            // this peer offered us the item; remove it from the list of items they offered us, and 
            // add it to the list of items we've offered them.  That will prevent us from offering them
            // the same item back (no reason to do that; we already know they have it)
            peer->inventory_peer_advertised_to_us.erase(iter);
            peer->inventory_advertised_to_peer.insert(block_message_item_id);
          }
        }
        message_propagation_data propagation_data{message_receive_time, message_validated_time, originating_peer->node_id};
        broadcast(block_message_to_process, propagation_data);
        _message_cache.block_accepted();
      }
      catch (fc::exception& e)
      {
        // client rejected the block.  Disconnect the client and any other clients that offered us this block
        wlog("client rejected block sent by peer");
        std::list<peer_connection_ptr> peers_to_disconnect;
        for (const peer_connection_ptr& peer : _active_connections)
          if (!peer->ids_of_items_to_get.empty() &&
              peer->ids_of_items_to_get.front() == block_message_to_process.block_id)
            peers_to_disconnect.push_back(peer);
        for (const peer_connection_ptr& peer : peers_to_disconnect)
        {
          wlog("disconnecting client ${endpoint} because it offered us the rejected block", ("endpoint", peer->get_remote_endpoint()));
          disconnect_from_peer(peer.get(), "You offered me a block that I have deemed to be invalid", true, e);
        }
      }
    }
    void node_impl::process_block_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash)
    {
      // find out whether we requested this item while we were synchronizing or during normal operation
      // (it's possible that we request an item during normal operation and then get kicked into sync
      // mode before we receive and process the item.  In that case, we should process the item as a normal
      // item to avoid confusing the sync code)
      bts::client::block_message block_message_to_process(message_to_process.as<bts::client::block_message>());
      auto item_iter = originating_peer->items_requested_from_peer.find(item_id(bts::client::block_message_type, message_hash));
      if (item_iter != originating_peer->items_requested_from_peer.end())
      {
        originating_peer->items_requested_from_peer.erase(item_iter);
        process_block_during_normal_operation(originating_peer, block_message_to_process, message_hash);
        return;
      }
      else
      {
        // not during normal operation.  see if we requested it during sync
        auto sync_item_iter = originating_peer->sync_items_requested_from_peer.find(item_id(bts::client::block_message_type, block_message_to_process.block_id));
        if (sync_item_iter != originating_peer->sync_items_requested_from_peer.end())
        {
          originating_peer->sync_items_requested_from_peer.erase(sync_item_iter);
          process_block_during_sync(originating_peer, block_message_to_process, message_hash);
          return;
        }
      }

      // if we get here, we didn't request the message, we must have a misbehaving peer
      wlog("received a block ${block_id} I didn't ask for from peer ${endpoint}, disconnecting from peer", 
            ("endpoint", originating_peer->get_remote_endpoint())
            ("block_id",block_message_to_process.block_id));
      fc::exception detailed_error(FC_LOG_MESSAGE(error, "You sent me a block that I didn't ask for, block_id: ${block_id}", 
                                                  ("block_id", block_message_to_process.block_id)
                                                  ("bitshares_git_revision_sha", originating_peer->bitshares_git_revision_sha)
                                                  ("bitshares_git_revision_unix_timestamp", originating_peer->bitshares_git_revision_unix_timestamp)
                                                  ("fc_git_revision_sha", originating_peer->fc_git_revision_sha)
                                                  ("fc_git_revision_unix_timestamp", originating_peer->fc_git_revision_unix_timestamp)));
      disconnect_from_peer(originating_peer, "You sent me a block that I didn't ask for", true, detailed_error);
    }

    void node_impl::on_current_time_request_message(peer_connection* originating_peer, const current_time_request_message& current_time_request_message_received)
    {
      fc::time_point request_received_time(fc::time_point::now());
      current_time_reply_message reply(current_time_request_message_received.request_sent_time,
                                       request_received_time,
                                       fc::time_point::now());
      originating_peer->send_message(reply);
    }

    void node_impl::on_current_time_reply_message(peer_connection* originating_peer, const current_time_reply_message& current_time_reply_message_received)
    {
      // TODO
    }

    void node_impl::on_check_firewall_message(peer_connection* originating_peer, const check_firewall_message& check_firewall_message_received)
    {
      // TODO
      check_firewall_reply_message reply;
      reply.node_id = check_firewall_message_received.node_id;
      reply.endpoint_checked = check_firewall_message_received.endpoint_to_check;
      reply.result = firewall_check_result::unable_to_check;
    }

    void node_impl::on_check_firewall_reply_message(peer_connection* originating_peer, const check_firewall_reply_message& check_firewall_reply_message_received)
    {
      // TODO
    }

    // this handles any message we get that doesn't require any special processing.
    // currently, this is any message other than block messages and p2p-specific
    // messages.  (transaction messages would be handled here, for example)
    // this just passes the message to the client, and does the bookkeeping 
    // related to requesting and rebroadcasting the message.
    void node_impl::process_ordinary_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash)
    {
      fc::time_point message_receive_time = fc::time_point::now();

      // only process it if we asked for it
      auto iter = originating_peer->items_requested_from_peer.find(item_id(message_to_process.msg_type, message_hash));
      if (iter == originating_peer->items_requested_from_peer.end())
      {
        wlog("received a message I didn't ask for from peer ${endpoint}, disconnecting from peer", 
             ("endpoint", originating_peer->get_remote_endpoint()));
        fc::exception detailed_error(FC_LOG_MESSAGE(error, "You sent me a message that I didn't ask for, message_hash: ${message_hash}", 
                                                    ("message_hash", message_hash)));
        disconnect_from_peer(originating_peer, "You sent me a message that I didn't request", true, detailed_error);
        return;
      }
      else
      {
        originating_peer->items_requested_from_peer.erase(iter);
        trigger_fetch_items_loop();

        // Next: have the delegate process the message
        fc::time_point message_validated_time;
        try
        {
          //bool message_caused_fork_switch = _delegate->handle_message(message_to_process, false);
          // for now, we assume an "ordinary" message won't cause us to switch forks (which
          // is currently the case.  if this changes, add some logic to handle it here)
          //assert(!message_caused_fork_switch);
          assert(!_delegate->handle_message(message_to_process, false));
          message_validated_time = fc::time_point::now();
        }
        catch (fc::exception& e)
        {
          wlog("client rejected block sent by peer ${peer}, ${e}", ("peer", originating_peer->get_remote_endpoint())("e", e.to_string()));
          return;
        }

        // finally, if the delegate validated the message, broadcast it to our other peers
        message_propagation_data propagation_data{message_receive_time, message_validated_time, originating_peer->node_id};
        broadcast(message_to_process, propagation_data);
      }
    }

    void node_impl::start_synchronizing_with_peer(const peer_connection_ptr& peer)
    {
      peer->we_need_sync_items_from_peer = true;
      peer->last_block_delegate_has_seen = item_hash_t();
      peer->last_block_number_delegate_has_seen = 0;
      fetch_next_batch_of_item_ids_from_peer(peer.get());
    }

    void node_impl::start_synchronizing()
    {
      for (const peer_connection_ptr& peer : _active_connections)
        start_synchronizing_with_peer(peer);
    }

    void node_impl::new_peer_just_added(const peer_connection_ptr& peer)
    {
      start_synchronizing_with_peer(peer);
      _delegate->connection_count_changed(_active_connections.size());
    }

    void node_impl::close()
    {

      _tcp_server.close();
      if (_accept_loop_complete.valid())
      {
        _accept_loop_complete.cancel();
        _accept_loop_complete.wait();
      }
    }

    void node_impl::accept_connection_task(peer_connection_ptr new_peer)
    {
      new_peer->accept_connection(); // this blocks until the secure connection is fully negotiated
      send_hello_message(new_peer);
    }

    void node_impl::accept_loop()
    {
      while (!_accept_loop_complete.canceled())
      {
        peer_connection_ptr new_peer(std::make_shared<peer_connection>(std::ref(*this)));
        try
        {
          _tcp_server.accept(new_peer->get_socket());
          ilog("accepted inbound connection from ${remote_endpoint}", ("remote_endpoint", new_peer->get_socket().remote_endpoint()));
          new_peer->connection_initiation_time = fc::time_point::now();
          _handshaking_connections.insert(new_peer);
          _rate_limiter.add_tcp_socket(&new_peer->get_socket());

          fc::async([=]() { accept_connection_task(new_peer); });

          // limit the rate at which we accept connections to mitigate DOS attacks
          fc::usleep(fc::microseconds(1000 * 10));
        }
        catch (const fc::exception& e)
        {
          elog("fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string()));
          throw;
        }
      }
    } // accept_loop()

    void node_impl::send_hello_message(const peer_connection_ptr& peer)
    {
      hello_message hello(_user_agent_string, 
                          core_protocol_version, 
                          peer->inbound_address,
                          peer->inbound_port, 
                          peer->outbound_port,
                          _node_id, 
                          _chain_id, 
                          generate_hello_user_data());

      peer->send_message(message(hello));
    }

    void node_impl::connect_to_task(peer_connection_ptr new_peer, const fc::ip::endpoint& remote_endpoint)
    {
      // create or find the database entry for the new peer
      // if we're connecting to them, we believe they're not firewalled
      potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(remote_endpoint);
      updated_peer_record.last_connection_disposition = last_connection_failed;
      updated_peer_record.last_connection_attempt_time = fc::time_point::now();;
      _potential_peer_db.update_entry(updated_peer_record);

      try
      {
        new_peer->connect_to(remote_endpoint, _actual_listening_endpoint);  // blocks until the connection is established and secure connection is negotiated

        // connection succeeded, we've started handshaking.  record that in our database
        updated_peer_record.last_connection_disposition = last_connection_handshaking_failed;
        updated_peer_record.number_of_successful_connection_attempts++;
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);
      }
      catch (fc::exception& except)
      {
        // connection failed.  record that in our database
        updated_peer_record.last_connection_disposition = last_connection_failed;
        updated_peer_record.number_of_failed_connection_attempts++;
        updated_peer_record.last_error = except;
        _potential_peer_db.update_entry(updated_peer_record);

        _handshaking_connections.erase(new_peer);
        display_current_connections();
        trigger_p2p_network_connect_loop();

        throw except;
      }

      fc::ip::endpoint local_endpoint = new_peer->get_local_endpoint();
      new_peer->inbound_address = local_endpoint.get_address();
      new_peer->inbound_port = _actual_listening_endpoint.port();
      new_peer->outbound_port = local_endpoint.port();

      new_peer->our_state = peer_connection::our_connection_state::just_connected;
      new_peer->their_state = peer_connection::their_connection_state::just_connected;
      send_hello_message(new_peer);
      dlog("Sent \"hello\" to peer ${peer}", ("peer", new_peer->get_remote_endpoint()));
    }

    // methods implementing node's public interface
    void node_impl::set_node_delegate(node_delegate* del)
    {
      _delegate = del;
      if (_delegate)
        _chain_id = del->get_chain_id();
    }

    void node_impl::load_configuration(const fc::path& configuration_directory)
    {
      _node_configuration_directory = configuration_directory;
      fc::path configuration_file_name(_node_configuration_directory / NODE_CONFIGURATION_FILENAME);
      bool node_configuration_loaded = false;
      if (fc::exists(configuration_file_name))
      {
        try
        {
          _node_configuration = fc::json::from_file(configuration_file_name).as<detail::node_configuration>();
          ilog("Loaded configuration from file ${filename}", ("filename", configuration_file_name));
          node_configuration_loaded = true;
        }
        catch (fc::parse_error_exception& parse_error)
        {
          elog("malformed node configuration file ${filename}: ${error}", 
               ("filename", configuration_file_name)("error", parse_error.to_detail_string()));
        }
        catch (fc::exception& except)
        {
          elog("unexpected exception while reading configuration file ${filename}: ${error}", 
               ("filename", configuration_file_name)("error", except.to_detail_string()));
        }
      }

      if (!node_configuration_loaded)      
      {
        _node_configuration = detail::node_configuration();
        ilog("generating new private key for this node");
        _node_configuration.listen_endpoint.set_port(BTS_NETWORK_DEFAULT_P2P_PORT);
        _node_configuration.wait_if_endpoint_is_busy = false;
        _node_configuration.private_key = fc::ecc::private_key::generate();
      }

      _node_id = _node_configuration.private_key.get_public_key().serialize();

      fc::path potential_peer_database_file_name(_node_configuration_directory / POTENTIAL_PEER_DATABASE_FILENAME);
      try
      {
        _potential_peer_db.open(potential_peer_database_file_name);
        trigger_p2p_network_connect_loop();
      }
      catch (fc::exception& except)
      {
        elog("unable to open peer database ${filename}: ${error}", 
             ("filename", potential_peer_database_file_name)("error", except.to_detail_string()));
        throw;
      }
    }

    void node_impl::listen_to_p2p_network()
    {
      assert(_node_id != fc::ecc::public_key_data());

      fc::ip::endpoint listen_endpoint = _node_configuration.listen_endpoint;
      if (listen_endpoint.port() != 0)
      {
        // if the user specified a port, we only want to bind to it if it's not already
        // being used by another application.  During normal operation, we set the
        // SO_REUSEADDR/SO_REUSEPORT flags so that we can bind outbound sockets to the
        // same local endpoint as we're listening on here.  On some platforms, setting 
        // those flags will prevent us from detecting that other applications are 
        // listening on that port.  We'd like to detect that, so we'll set up a temporary
        // tcp server without that flag to see if we can listen on that port.
        bool first = true;
        for (;;)
        {
          try
          {
            fc::tcp_server temporary_server;
            if (listen_endpoint.get_address() != fc::ip::address())
              temporary_server.listen(listen_endpoint);
            else
              temporary_server.listen(listen_endpoint.port());
            break;
          }
          catch (fc::exception&)
          {
            if (_node_configuration.wait_if_endpoint_is_busy)
            {
              std::ostringstream error_message;
              //error_message << "Unable to listen for connections on port " << _node_configuration.listen_endpoint.port()
              //              << ", retrying in a few seconds";
              // I think the right thing to do here is to send the delegate an error_encountered message: 
              //   _delegate->error_encountered(error_message.str(), fc::oexception());
              // but we don't have the CLI fully initialized at this point, so the message gets discarded.
              // for now, just cout it
              if (first)
              {
                std::cout << "Unable to listen for connections on port " << listen_endpoint.port() 
                          << ", retrying in a few seconds\n";
                std::cout << "You can wait for it to become available, or restart this program using\n";
                std::cout << "the --p2p-port option to specify another port\n";
                first = false;
              }
              else
              {
                std::cout << "\nStill waiting for port " << listen_endpoint.port() << " to become available\n";
              }
              fc::usleep(fc::seconds(5));
            }
            else // don't wait, just find a random port
            {
              wlog("unable to bind on the requested endpoint ${endpoint}, which probably means that endpoint is already in use",
                   ("endpoint", listen_endpoint));
              listen_endpoint.set_port(0);
            }
          }
        }
      }
      else // port is 0
      {
        // if they requested a random port, we'll just assume it's available
        // (it may not be due to ip address, but we'll detect that in the next step)
      }

      _tcp_server.set_reuse_address();
      try
      {
        if (listen_endpoint.get_address() != fc::ip::address())
          _tcp_server.listen(listen_endpoint);
        else
          _tcp_server.listen(listen_endpoint.port());
        _actual_listening_endpoint = _tcp_server.get_local_endpoint();
        ilog("listening for connections on endpoint ${endpoint} (our first choice)", 
              ("endpoint", _actual_listening_endpoint));
      }
      catch (fc::exception& e)
      {
        FC_RETHROW_EXCEPTION(e, error, "unable to listen on ${endpoint}", ("endpoint",listen_endpoint));
      }
    }

    void node_impl::connect_to_p2p_network()
    {
      assert(_node_id != fc::ecc::public_key_data());

      _accept_loop_complete = fc::async( [=](){ accept_loop(); });

      _p2p_network_connect_loop_done = fc::async([=]() { p2p_network_connect_loop(); });
      _fetch_sync_items_loop_done = fc::async([=]() { fetch_sync_items_loop(); });
      _fetch_item_loop_done = fc::async([=]() { fetch_items_loop(); });
      _advertise_inventory_loop_done = fc::async([=]() { advertise_inventory_loop(); });
      _terminate_inactive_connections_loop_done = fc::async([=]() { terminate_inactive_connections_loop(); });
    }

    void node_impl::add_node(const fc::ip::endpoint& ep)
    {
      // if we're connecting to them, we believe they're not firewalled
      potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(ep);

      // if we've recently connected to this peer, reset the last_connection_attempt_time to allow 
      // us to immediately retry this peer
      updated_peer_record.last_connection_attempt_time = std::min<fc::time_point_sec>(updated_peer_record.last_connection_attempt_time, 
                                                                                      fc::time_point::now() - fc::seconds(_peer_connection_retry_timeout));
      _add_once_node_list.push_back(updated_peer_record);
      _potential_peer_db.update_entry(updated_peer_record);
      trigger_p2p_network_connect_loop();
    }

    void node_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      if (is_connection_to_endpoint_in_progress(remote_endpoint))
        FC_THROW("already connected to requested endpoint ${endpoint}", ("endpoint", remote_endpoint));

      dlog("node_impl::connect_to(${endpoint})", ("endpoint", remote_endpoint));
      peer_connection_ptr new_peer(std::make_shared<peer_connection>(std::ref(*this)));
      new_peer->get_socket().open();
      new_peer->get_socket().set_reuse_address();
      new_peer->set_remote_endpoint(remote_endpoint);
      new_peer->connection_initiation_time = fc::time_point::now();
      _handshaking_connections.insert(new_peer);
      _rate_limiter.add_tcp_socket(&new_peer->get_socket());
      fc::async([=](){ connect_to_task(new_peer, remote_endpoint); });
    }

    peer_connection_ptr node_impl::get_connection_to_endpoint(const fc::ip::endpoint& remote_endpoint)
    {
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_this_peer(active_peer->get_remote_endpoint());
        if (endpoint_for_this_peer && *endpoint_for_this_peer == remote_endpoint)
          return active_peer;
      }
      for (const peer_connection_ptr& handshaking_peer : _handshaking_connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_this_peer(handshaking_peer->get_remote_endpoint());
        if (endpoint_for_this_peer && *endpoint_for_this_peer == remote_endpoint)
          return handshaking_peer;
      }
      return peer_connection_ptr();
    }

    bool node_impl::is_connection_to_endpoint_in_progress(const fc::ip::endpoint& remote_endpoint)
    {
      return get_connection_to_endpoint(remote_endpoint) != peer_connection_ptr();
    }

    void node_impl::dump_node_status()
    {
      dlog("----------------- PEER STATUS UPDATE --------------------");
      dlog(" number of peers: ${active} active, ${handshaking}, ${closing} closing.  attempting to maintain ${desired} - ${maximum} peers", 
           ("active", _active_connections.size())("handshaking", _handshaking_connections.size())("closing",_closing_connections.size())
           ("desired", _desired_number_of_connections)("maximum", _maximum_number_of_connections));
      for (const peer_connection_ptr& peer : _active_connections)
      {
        dlog("       active peer ${endpoint} peer_is_in_sync_with_us:${in_sync_with_us} we_are_in_sync_with_peer:${in_sync_with_them}", 
             ("endpoint", peer->get_remote_endpoint())
             ("in_sync_with_us", !peer->peer_needs_sync_items_from_us)("in_sync_with_them", !peer->we_need_sync_items_from_peer));
        if (peer->we_need_sync_items_from_peer)
          dlog("              above peer has ${count} sync items we might need", ("count", peer->ids_of_items_to_get.size()));
      }
      for (const peer_connection_ptr& peer : _handshaking_connections)
      {
        dlog("  handshaking peer ${endpoint} in state ours(${our_state}) theirs(${their_state})", 
             ("endpoint", peer->get_remote_endpoint())("our_state", peer->our_state)("their_state", peer->their_state));
      }

      dlog("--------- MEMORY USAGE ------------");
      dlog("node._active_sync_requests size: ${size} (this is known to be broken)", ("size", _active_sync_requests.size())); // TODO: un-break this
      dlog("node._received_sync_items size: ${size}", ("size", _received_sync_items.size()));
      dlog("node._items_to_fetch size: ${size}", ("size", _items_to_fetch.size()));
      dlog("node._new_inventory size: ${size}", ("size", _new_inventory.size()));
      dlog("node._message_cache size: ${size}", ("size", _message_cache.size()));
      for (const peer_connection_ptr& peer : _active_connections)
      {
        dlog("  peer ${endpoint}", ("endpoint", peer->get_remote_endpoint()));
        dlog("    peer.ids_of_items_to_get size: ${size}", ("size", peer->ids_of_items_to_get.size()));
        dlog("    peer.inventory_peer_advertised_to_us size: ${size}", ("size", peer->inventory_peer_advertised_to_us.size()));
        dlog("    peer.inventory_advertised_to_peer size: ${size}", ("size", peer->inventory_advertised_to_peer.size()));
        dlog("    peer.items_requested_from_peer size: ${size}", ("size", peer->items_requested_from_peer.size()));
        dlog("    peer.sync_items_requested_from_peer size: ${size}", ("size", peer->sync_items_requested_from_peer.size()));
      }
      dlog("--------- END MEMORY USAGE ------------");
    }

    void node_impl::disconnect_from_peer( peer_connection* peer_to_disconnect,
                                          const std::string& reason_for_disconnect,
                                          bool caused_by_error /* = false */,
                                          const fc::oexception& error /* = fc::oexception() */)
    {
      _closing_connections.insert(peer_to_disconnect->shared_from_this());
      _handshaking_connections.erase(peer_to_disconnect->shared_from_this());
      _active_connections.erase(peer_to_disconnect->shared_from_this());

      if (peer_to_disconnect->they_have_requested_close)
      {
        // the peer has already told us that it's ready to close the connection, so just close the connection
        peer_to_disconnect->close_connection();
      }
      else
      {
        // we're the first to try to want to close the connection
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*peer_to_disconnect->get_remote_endpoint());
        updated_peer_record.last_seen_time = fc::time_point::now();
        if (error)
          updated_peer_record.last_error = error;
        else
          updated_peer_record.last_error = fc::exception(FC_LOG_MESSAGE(info, reason_for_disconnect.c_str()));
        _potential_peer_db.update_entry(updated_peer_record);

        peer_to_disconnect->we_have_requested_close = true;
        peer_to_disconnect->connection_closed_time = fc::time_point::now();

        closing_connection_message closing_message(reason_for_disconnect, caused_by_error, error);
        peer_to_disconnect->send_message(closing_message);
      }

      // notify the user.  This will be useful in testing, but we might want to remove it later;
      // it makes good sense to notify the user if other nodes think she is behaving badly, but
      // if we're just detecting and dissconnecting other badly-behaving nodes, they don't really care.
      if (caused_by_error)
      {
        std::ostringstream error_message;
        error_message << "I am disconnecting peer " << fc::variant(peer_to_disconnect->get_remote_endpoint()).as_string() <<
                         " for reason: " << reason_for_disconnect;
        _delegate->error_encountered(error_message.str(), fc::oexception());
      }

      // peer_to_disconnect->close_connection();
    }

    void node_impl::listen_on_endpoint(const fc::ip::endpoint& ep)
    {
      _node_configuration.listen_endpoint = ep;
      save_node_configuration();
    }

    void node_impl::listen_on_port(uint16_t port, bool wait_if_not_available)
    {
      _node_configuration.listen_endpoint = fc::ip::endpoint(fc::ip::address(), port);
      _node_configuration.wait_if_endpoint_is_busy = wait_if_not_available;
      save_node_configuration();
    }

    fc::ip::endpoint node_impl::get_actual_listening_endpoint() const
    {
      return _actual_listening_endpoint;
    }

    std::vector<peer_status> node_impl::get_connected_peers() const
    {
      std::vector<peer_status> statuses;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        peer_status this_peer_status;
        this_peer_status.version = 0; // TODO
        fc::optional<fc::ip::endpoint> endpoint = peer->get_remote_endpoint();
        if (endpoint)
          this_peer_status.host = *endpoint;
        fc::mutable_variant_object peer_details;
        peer_details["addr"] = endpoint ? (std::string)*endpoint : std::string();
        peer_details["addrlocal"] = (std::string)peer->get_local_endpoint();
        peer_details["services"] = "00000001"; // TODO: assign meaning, right now this just prints what bitcoin prints
        peer_details["lastsend"] = peer->get_last_message_sent_time().sec_since_epoch();
        peer_details["lastrecv"] = peer->get_last_message_received_time().sec_since_epoch();
        peer_details["bytessent"] = peer->get_total_bytes_sent();
        peer_details["bytesrecv"] = peer->get_total_bytes_received();
        peer_details["conntime"] = peer->get_connection_time(); 
        peer_details["pingtime"] = ""; // TODO: fill me for bitcoin compatibility
        peer_details["pingwait"] = ""; // TODO: fill me for bitcoin compatibility
        peer_details["version"] = ""; // TODO: fill me for bitcoin compatibility
        peer_details["subver"] = peer->user_agent;
        peer_details["inbound"] = peer->direction == peer_connection_direction::inbound;
        peer_details["firewall_status"] = peer->is_firewalled;
        peer_details["startingheight"] = ""; // TODO: fill me for bitcoin compatibility
        peer_details["banscore"] = ""; // TODO: fill me for bitcoin compatibility
        peer_details["syncnode"] = ""; // TODO: fill me for bitcoin compatibility

        if (peer->bitshares_git_revision_sha)
        {
          std::string revision_string = *peer->bitshares_git_revision_sha;
          if (*peer->bitshares_git_revision_sha == bts::utilities::git_revision_sha)
            revision_string += " (same as ours)";
          else
            revision_string += " (different from ours)";
          peer_details["bitshares_git_revision_sha"] = revision_string;

        }
        if (peer->bitshares_git_revision_unix_timestamp)
        {
          peer_details["bitshares_git_revision_unix_timestamp"] = *peer->bitshares_git_revision_unix_timestamp;
          std::string age_string = fc::get_approximate_relative_time_string(*peer->bitshares_git_revision_unix_timestamp);
          if (*peer->bitshares_git_revision_unix_timestamp == fc::time_point_sec(bts::utilities::git_revision_unix_timestamp))
            age_string += " (same as ours)";
          else if (*peer->bitshares_git_revision_unix_timestamp > fc::time_point_sec(bts::utilities::git_revision_unix_timestamp))
            age_string += " (newer than ours)";
          else
            age_string += " (older than ours)";
          peer_details["bitshares_git_revision_age"] = age_string;
        }
        
        if (peer->fc_git_revision_sha)
        {
          std::string revision_string = *peer->fc_git_revision_sha;
          if (*peer->fc_git_revision_sha == fc::git_revision_sha)
            revision_string += " (same as ours)";
          else
            revision_string += " (different from ours)";
          peer_details["fc_git_revision_sha"] = revision_string;

        }
        if (peer->fc_git_revision_unix_timestamp)
        {
          peer_details["fc_git_revision_unix_timestamp"] = *peer->fc_git_revision_unix_timestamp;
          std::string age_string = fc::get_approximate_relative_time_string(*peer->fc_git_revision_unix_timestamp);
          if (*peer->fc_git_revision_unix_timestamp == fc::time_point_sec(fc::git_revision_unix_timestamp))
            age_string += " (same as ours)";
          else if (*peer->fc_git_revision_unix_timestamp > fc::time_point_sec(fc::git_revision_unix_timestamp))
            age_string += " (newer than ours)";
          else
            age_string += " (older than ours)";
          peer_details["fc_git_revision_age"] = age_string;
        }

        if (peer->platform)
          peer_details["platform"] = *peer->platform;

        this_peer_status.info = peer_details;
        statuses.push_back(this_peer_status);
      }
      return statuses;
    }

    uint32_t node_impl::get_connection_count() const
    {
      return _active_connections.size();
    }

    void node_impl::broadcast(const message& item_to_broadcast, const message_propagation_data& propagation_data)
    {
      fc::uint160_t hash_of_message_contents;
      if (item_to_broadcast.msg_type == bts::client::block_message_type)
      {
        bts::client::block_message block_message_to_broadcast = item_to_broadcast.as<bts::client::block_message>();
        hash_of_message_contents = block_message_to_broadcast.block_id; // for debugging
        _most_recent_blocks_accepted.push_back(block_message_to_broadcast.block_id);
      }
      else if (item_to_broadcast.msg_type == bts::client::trx_message_type)
      {
        bts::client::trx_message transaction_message_to_broadcast = item_to_broadcast.as<bts::client::trx_message>();
        hash_of_message_contents = transaction_message_to_broadcast.trx.id(); // for debugging
      }
      message_hash_type hash_of_item_to_broadcast = item_to_broadcast.id();

      _message_cache.cache_message(item_to_broadcast, hash_of_item_to_broadcast, propagation_data, hash_of_message_contents);
      _new_inventory.insert(item_id(item_to_broadcast.msg_type, hash_of_item_to_broadcast));
      trigger_advertise_inventory_loop();
      dump_node_status();
    }

    void node_impl::broadcast(const message& item_to_broadcast)
    {
      // this version is called directly from the client
      message_propagation_data propagation_data{fc::time_point::now(), fc::time_point::now(), _node_id};
      broadcast(item_to_broadcast, propagation_data);
    }

    void node_impl::sync_from(const item_id& last_item_id_seen)
    {
      _most_recent_blocks_accepted.clear();
      _sync_item_type = last_item_id_seen.item_type;
      _most_recent_blocks_accepted.push_back(last_item_id_seen.item_hash);
    }

    bool node_impl::is_connected() const
    {
      return !_active_connections.empty();
    }

    void node_impl::set_advanced_node_parameters(const fc::variant_object& params)
    {
      if (params.contains("peer_connection_retry_timeout"))
        _peer_connection_retry_timeout = (uint32_t)params["peer_connection_retry_timeout"].as_uint64();
      if (params.contains("desired_number_of_connections"))
        _desired_number_of_connections = (uint32_t)params["desired_number_of_connections"].as_uint64();
      if (params.contains("maximum_number_of_connections"))
        _maximum_number_of_connections = (uint32_t)params["maximum_number_of_connections"].as_uint64();

      _desired_number_of_connections = std::min(_desired_number_of_connections, _maximum_number_of_connections);

      while (_active_connections.size() > _maximum_number_of_connections)
        disconnect_from_peer(_active_connections.begin()->get(),
                             "I have too many connections open");
      trigger_p2p_network_connect_loop();
    }

    fc::variant_object node_impl::get_advanced_node_parameters()
    {
      fc::mutable_variant_object result;
      result["peer_connection_retry_timeout"] = _peer_connection_retry_timeout;
      result["desired_number_of_connections"] = _desired_number_of_connections;
      result["maximum_number_of_connections"] = _maximum_number_of_connections;
      return result;
    }

    message_propagation_data node_impl::get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
    {
      return _message_cache.get_message_propagation_data(transaction_id);
    }

    message_propagation_data node_impl::get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
    {
      return _message_cache.get_message_propagation_data(block_id);
    }

    node_id_t node_impl::get_node_id() const
    {
      return _node_id;
    }
    void node_impl::set_allowed_peers(const std::vector<node_id_t>& allowed_peers)
    {
#ifdef ENABLE_P2P_DEBUGGING_API
      _allowed_peers.clear();
      _allowed_peers.insert(allowed_peers.begin(), allowed_peers.end());
      std::list<peer_connection_ptr> peers_to_disconnect;
      if (!_allowed_peers.empty())
        for (const peer_connection_ptr& peer : _active_connections)
          if (_allowed_peers.find(peer->node_id) == _allowed_peers.end())
            peers_to_disconnect.push_back(peer);
      for (const peer_connection_ptr& peer : peers_to_disconnect)
        disconnect_from_peer(peer.get(), "My allowed_peers list has changed, and you're no longer allowed.  Bye.");
#endif // ENABLE_P2P_DEBUGGING_API
    }
    void node_impl::clear_peer_database()
    {
      _potential_peer_db.clear();
    }

    void node_impl::set_total_bandwidth_limit(uint32_t upload_bytes_per_second, uint32_t download_bytes_per_second)
    {
      _rate_limiter.set_upload_limit(upload_bytes_per_second);
      _rate_limiter.set_download_limit(download_bytes_per_second);
    }

    fc::variant_object node_impl::network_get_info() const
    {
      fc::mutable_variant_object info;
      info["listening_on"] = _actual_listening_endpoint;
      info["node_id"] = _node_id;
      return info;
    }

  }  // end namespace detail



  ///////////////////////////////////////////////////////////////////////
  // implement node functions, they just delegate to detail::node_impl //

  node::node() : 
    my(new detail::node_impl())
  {
  }

  node::~node()
  {
  }

  void node::set_node_delegate(node_delegate* del)
  {
    my->set_node_delegate(del);
  }

  void node::load_configuration(const fc::path& configuration_directory)
  {
    my->load_configuration(configuration_directory);
  }

  void node::listen_to_p2p_network()
  {
    my->listen_to_p2p_network();
  }

  void node::connect_to_p2p_network()
  {
    my->connect_to_p2p_network();
  }

  void node::add_node(const fc::ip::endpoint& ep)
  {
    my->add_node(ep);
  }

  void node::connect_to(const fc::ip::endpoint& remote_endpoint)
  {
    my->connect_to(remote_endpoint);
  }

  void node::listen_on_endpoint(const fc::ip::endpoint& ep)
  {
    my->listen_on_endpoint(ep);
  }

  void node::listen_on_port(uint16_t port, bool wait_if_not_available)
  {
    my->listen_on_port(port, wait_if_not_available);
  }

  fc::ip::endpoint node::get_actual_listening_endpoint() const
  {
    return my->get_actual_listening_endpoint();
  }

  std::vector<peer_status> node::get_connected_peers() const
  {
    return my->get_connected_peers();
  }

  uint32_t node::get_connection_count() const
  {
    return my->get_connection_count();
  }

  void node::broadcast(const message& msg)
  {
    my->broadcast(msg);
  }

  void node::sync_from(const item_id& id)
  {
    my->sync_from(id);
  }

  bool node::is_connected() const
  {
    return my->is_connected();
  }

  std::vector<potential_peer_record> node::get_potential_peers()const
  {
     std::vector<potential_peer_record> result;
     for( auto itr = my->_potential_peer_db.begin(); itr != my->_potential_peer_db.end(); ++itr )
     {
        result.push_back(*itr);
     }
     return result;
  }

  void node::set_advanced_node_parameters(const fc::variant_object& params)
  {
    my->set_advanced_node_parameters(params);
  }

  fc::variant_object node::get_advanced_node_parameters()
  {
    return my->get_advanced_node_parameters();
  }

  message_propagation_data node::get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
  {
    return my->get_transaction_propagation_data(transaction_id);
  }
  message_propagation_data node::get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
  {
    return my->get_block_propagation_data(block_id);
  }
  node_id_t node::get_node_id() const
  {
    return my->get_node_id();
  }
  void node::set_allowed_peers(const std::vector<node_id_t>& allowed_peers)
  {
    my->set_allowed_peers(allowed_peers);
  }
  void node::clear_peer_database()
  {
    my->clear_peer_database();
  }

  void node::set_total_bandwidth_limit(uint32_t upload_bytes_per_second, 
                                       uint32_t download_bytes_per_second)
  {
    my->set_total_bandwidth_limit(upload_bytes_per_second, download_bytes_per_second);
  }

  fc::variant_object node::network_get_info() const
  {
    return my->network_get_info();
  }

  void simulated_network::broadcast( const message& item_to_broadcast )
  {
      for(node_delegate* network_node : network_nodes)
        network_node->handle_message(item_to_broadcast, false);
  }

  void simulated_network::add_node_delegate(node_delegate* node_delegate_to_add)
  { 
     network_nodes.push_back(node_delegate_to_add);
  }      
} } // end namespace bts::net
