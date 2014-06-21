#pragma once

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


