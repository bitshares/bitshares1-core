#include <deque>
#include <boost/tuple/tuple.hpp>

#include <fc/thread/thread.hpp>
#include <fc/thread/future.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/rand.hpp>

#include <bts/net/node.hpp>
#include <bts/net/peer_database.hpp>
#include <bts/net/message_oriented_connection.hpp>
#include <bts/net/stcp_socket.hpp>

namespace bts { namespace net {
  namespace detail
  {
    enum peer_connection_direction { unknown, inbound, outbound };

    class peer_connection : public message_oriented_connection_delegate,
                            public std::enable_shared_from_this<peer_connection>
    {
    public:
      enum connection_state
      {
        disconnected, 
        just_connected, // if inbound
        secure_connection_established, // ecdh complete
        hello_sent, // if outbound
        hello_reply_sent, // if inbound
        connection_rejected_sent, // if inbound
        connected,
        connection_rejected // if outbound
      };

    private:
      node_impl&                     _node;
      fc::optional<fc::ip::endpoint> _remote_endpoint;
      message_oriented_connection    _message_connection;
    public:
      peer_connection_direction direction;
      connection_state state;

      /// data about the peer node
      /// @{
      fc::uint160_t    node_id;
      uint32_t         core_protocol_version;
      std::string      user_agent;
      fc::ip::endpoint inbound_endpoint;
      /// @}

      /// blockchain synchronization state data
      /// @{
      std::deque<item_hash_t> ids_of_items_to_get; /// id of items in the blockchain that this peer has told us about
      uint32_t number_of_unfetched_item_ids; /// number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their ids
      bool peer_needs_sync_items_from_us;
      bool we_need_sync_items_from_peer;
      fc::optional<boost::tuple<item_id, fc::time_point> > item_ids_requested_from_peer; /// we check this to detect a timed-out request and in busy()
      /// @}

      /// non-syncronization state data
      /// @{
      std::unordered_set<item_id> inventory_peer_advertised_to_us;
      std::unordered_set<item_id> inventory_advertised_to_peer; /// TODO: make this a map to the time/block# we advertised it so we can expire items off of the list

      std::unordered_map<item_id, fc::time_point> items_requested_from_peer;  /// fetch from another peer if this peer disconnects
      /// @}
    public:
      peer_connection(node_impl& n) : 
        _message_connection(this),
        _node(n),
        direction(unknown),
        state(disconnected),
        number_of_unfetched_item_ids(0),
        peer_needs_sync_items_from_us(true),
        we_need_sync_items_from_peer(true)
      {}
      ~peer_connection() {}

      fc::tcp_socket& get_socket();
      void accept_connection();
      void connect_to(const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint = fc::optional<fc::ip::endpoint>());

      void on_message(message_oriented_connection* originating_connection, const message& received_message) override;
      void on_connection_closed(message_oriented_connection* originating_connection) override;

      void send_message(const message& message_to_send);
      void close_connection();

      fc::optional<fc::ip::endpoint> get_remote_endpoint();
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
    };

 } } } // end namespace bts::net::detail

FC_REFLECT(bts::net::detail::node_configuration, (listen_endpoint));

// not sent over the wire, just reflected for logging
FC_REFLECT_ENUM(bts::net::detail::peer_connection_direction, (unknown)(inbound)(outbound))

namespace bts { namespace net { 
  namespace detail 
  {
    class node_impl
    {
    public:
      node_delegate*       _delegate;

#define NODE_CONFIGURATION_FILENAME      "node_config.json"
#define POTENTIAL_PEER_DATABASE_FILENAME "peers.leveldb"
      fc::path             _node_configuration_directory;
      node_configuration   _node_configuration;

      peer_database        _potential_peer_db;

      fc::promise<void>::ptr _retrigger_connect_loop_promise;
      bool                   _potential_peer_database_updated;

      std::string          _user_agent_string;
      fc::uint160_t        _node_id;

      /** if we have less than `_desired_number_of_connections`, we will try to connect with more nodes */
      uint32_t             _desired_number_of_connections;
      /** if we have _maximum_number_of_connections or more, we will refuse any inbound connections */
      uint32_t             _maximum_number_of_connections;

      fc::tcp_server       _tcp_server;
      fc::future<void>     _accept_loop_complete;

      /** Stores all connections which have not yet finished key exchange or are still sending initial handshaking messages
       * back and forth (not yet ready to initiate syncing) */
      std::unordered_set<peer_connection_ptr>                     _handshaking_connections;
      /** stores fully established connections we're either syncing with or in normal operation with */
      std::unordered_set<peer_connection_ptr>                     _active_connections;
      /** stores connections we've closed, but are still waiting for the remote end to close before we delete them */
      std::unordered_set<peer_connection_ptr>                     _closing_connections;

      fc::future<void> _p2p_network_connect_loop_done;

      node_impl();
      ~node_impl();

      void save_node_configuration();

      void p2p_network_connect_loop();
      void trigger_p2p_network_connect_loop();

      bool is_accepting_new_connections();
      bool is_wanting_new_connections();
      uint32_t get_number_of_connections();

      bool is_already_connected_to_id(const fc::uint160_t node_id);
      bool merge_address_info_with_potential_peer_database(const std::vector<address_info> addresses);
      void display_current_connections();

      void on_message(peer_connection* originating_peer, const message& received_message);
      void on_hello_message(peer_connection* originating_peer, const hello_message& hello_message_received);
      void on_hello_reply_message(peer_connection* originating_peer, const hello_reply_message& hello_reply_message_received);
      void on_connection_rejected_message(peer_connection* originating_peer, const connection_rejected_message& connection_rejected_message_received);
      void on_address_request_message(peer_connection* originating_peer, const address_request_message& address_request_message_received);
      void on_address_message(peer_connection* originating_peer, const address_message& address_message_received);

      void on_connection_closed(peer_connection* originating_peer);

      void close();

      void accept_connection_task(peer_connection_ptr new_peer);
      void accept_loop();
      void connect_to_task(peer_connection_ptr new_peer, const fc::ip::endpoint& remote_endpoint);
      bool is_connection_to_endpoint_in_progress(const fc::ip::endpoint& remote_endpoint);

      // methods implementing node's public interface
      void set_delegate(node_delegate* del);
      void load_configuration(const fc::path& configuration_directory);
      void connect_to_p2p_network();
      void add_node(const fc::ip::endpoint& ep);
      void connect_to(const fc::ip::endpoint& ep);
      void listen_on_endpoint(const fc::ip::endpoint& ep);
      void listen_on_port(uint16_t port);
      std::vector<peer_status> get_connected_peers() const;
      void broadcast(const message& item_to_broadcast);
      void sync_from(const item_id&);
      bool is_connected() const;
    }; // end class node_impl

    fc::tcp_socket& peer_connection::get_socket()
    {
      return _message_connection.get_socket();
    }

    void peer_connection::accept_connection()
    {
      try
      {
        assert(state == disconnected);
        direction = inbound;
        state = peer_connection::just_connected;
        _message_connection.accept();           // perform key exchange
        _remote_endpoint = _message_connection.get_socket().remote_endpoint();
        state = peer_connection::secure_connection_established;
        ilog("established inbound connection from ${remote_endpoint}", ("remote_endpoint", _message_connection.get_socket().remote_endpoint()));
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
        assert(state == disconnected);
        direction = outbound;

        _remote_endpoint = remote_endpoint;
        if (local_endpoint)
          _message_connection.connect_to(remote_endpoint, *local_endpoint);
        else
          _message_connection.connect_to(remote_endpoint);
        state = peer_connection::secure_connection_established;
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

    fc::optional<fc::ip::endpoint> peer_connection::get_remote_endpoint()
    {
      return _remote_endpoint;
    }
    void peer_connection::set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint)
    {
      _remote_endpoint = new_remote_endpoint;
    }

    bool peer_connection::busy() 
    { 
      return !items_requested_from_peer.empty() || item_ids_requested_from_peer;
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
      _desired_number_of_connections(3),
      _maximum_number_of_connections(5)
    {
      fc::rand_pseudo_bytes(_node_id.data(), 20);
    }

    node_impl::~node_impl()
    {
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
          elog("error writing node configuration to file ${filename}: ${error}", ("filename", configuration_file_name)("error", except.to_detail_string()));
        }
      }
    }

    void node_impl::p2p_network_connect_loop()
    {
      for (;;)
      {
        ilog("Starting an iteration of p2p_network_connect_loop().");
        display_current_connections();

        while (is_wanting_new_connections())
        {
          bool initiated_connection_this_pass = false;
          _potential_peer_database_updated = false;

          for (auto iter = _potential_peer_db.begin();
               iter != _potential_peer_db.end() && is_wanting_new_connections();
               ++iter)
          {
            ilog("Last attempt was ${time_distance} seconds ago (disposition: ${disposition})", ("time_distance", (fc::time_point::now() - iter->last_connection_attempt_time).count() / fc::seconds(1).count())("disposition", iter->last_connection_disposition));
            if (!is_connection_to_endpoint_in_progress(iter->endpoint) &&
                ((iter->last_connection_disposition != last_connection_failed && iter->last_connection_disposition != last_connection_rejected) ||
                 iter->last_connection_attempt_time < fc::time_point::now() - fc::seconds(60 * 5)))
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
          if (is_wanting_new_connections())
          {
            ilog("Still want to connect to more nodes, but I don't have any good candidates.  Trying again in 15 seconds");
            _retrigger_connect_loop_promise->wait_until(fc::time_point::now() + fc::seconds(15));
          }
          else
          {
            ilog("I don't need any more connections, waiting forever until something changes");
            _retrigger_connect_loop_promise->wait();
          }
        }
        catch (fc::timeout_exception&)
        {
          // we timed out; loop around and try retry 
        }
        _retrigger_connect_loop_promise.reset();
      }
    }

    void node_impl::trigger_p2p_network_connect_loop()
    {
      ilog("Triggering connect loop now");
      _potential_peer_database_updated = true;
      if (_retrigger_connect_loop_promise)
        _retrigger_connect_loop_promise->set_value();
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

    bool node_impl::is_already_connected_to_id(const fc::uint160_t node_id)
    {
      if (_node_id == node_id)
      {
        ilog("is_already_connected_to_id returning true because the peer us");
        return true;
      }
      for (const peer_connection_ptr active_peer : _active_connections)
        if (active_peer->node_id == node_id)
        {
          ilog("is_already_connected_to_id returning true because the peer is already in our active list");
          return true;
        }
      for (const peer_connection_ptr handshaking_peer : _handshaking_connections)
        if (handshaking_peer->node_id == node_id)
        {
          ilog("is_already_connected_to_id returning true because the peer is already in our handshaking list");
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
      ilog("Currently have ${current} of ${desired} connections", 
            ("current", get_number_of_connections())
            ("desired", _desired_number_of_connections));
      ilog("   my id is ${id}", ("id", _node_id));

      for (const peer_connection_ptr& active_connection : _active_connections)
      {
        ilog("        active: ${endpoint} with ${id}   [${direction}]", ("endpoint",active_connection->get_remote_endpoint())("id",active_connection->node_id)("direction",active_connection->direction));
      }
      for (const peer_connection_ptr& handshaking_connection : _handshaking_connections)
      {
        ilog("   handshaking: ${endpoint} with ${id}  [${direction}]", ("endpoint",handshaking_connection->get_remote_endpoint())("id",handshaking_connection->node_id)("direction",handshaking_connection->direction));
      }
    }

    void node_impl::on_message(peer_connection* originating_peer, const message& received_message)
    {
      message_hash_type message_hash = received_message.id();
      switch (received_message.msg_type)
      {
      case core_message_type_enum::hello_message_type:
        on_hello_message(originating_peer, received_message.as<hello_message>());
        break;
      case core_message_type_enum::hello_reply_message_type:
        on_hello_reply_message(originating_peer, received_message.as<hello_reply_message>());
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
      }
    }

    void node_impl::on_hello_message(peer_connection* originating_peer, const hello_message& hello_message_received)
    {
      bool already_connected_to_this_peer = is_already_connected_to_id(hello_message_received.node_id);

      // store off the data provided in the hello message
      originating_peer->node_id = hello_message_received.node_id;
      originating_peer->core_protocol_version = hello_message_received.core_protocol_version;
      originating_peer->inbound_endpoint = hello_message_received.inbound_endpoint;
      // hack: right now, a peer listening on all interfaces will tell us it is listening on 0.0.0.0, patch that up here:
      if (originating_peer->inbound_endpoint.get_address() == fc::ip::address())
        originating_peer->inbound_endpoint = fc::ip::endpoint(originating_peer->get_socket().remote_endpoint().get_address() ,originating_peer->inbound_endpoint.port());
      originating_peer->user_agent = hello_message_received.user_agent;

      // now decide what to do with it
      if (originating_peer->state == peer_connection::secure_connection_established && 
          originating_peer->direction == peer_connection_direction::inbound)
      {
        if (!is_accepting_new_connections())
        {
          connection_rejected_message connection_rejected(_user_agent_string, core_protocol_version, originating_peer->get_socket().remote_endpoint());
          originating_peer->send_message(message(connection_rejected));
          originating_peer->state = peer_connection::connection_rejected_sent;
          ilog("Received a hello_message from peer but I'm not accepting any more connections, rejection");
        }
        else if (already_connected_to_this_peer)
        {
          connection_rejected_message connection_rejected(_user_agent_string, core_protocol_version, originating_peer->get_socket().remote_endpoint());
          originating_peer->send_message(message(connection_rejected));
          originating_peer->state = peer_connection::connection_rejected_sent;
          ilog("Received a hello_message from a peer I'm already connected to,  rejection");
        }
        else
        {
          // they've told us what their public IP/endpoint is, add it to our peer database
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(originating_peer->inbound_endpoint);
          _potential_peer_db.update_entry(updated_peer_record);

          hello_reply_message hello_reply(_user_agent_string, core_protocol_version, originating_peer->get_socket().remote_endpoint(), _node_id);
          originating_peer->send_message(message(hello_reply));
          originating_peer->state = peer_connection::hello_reply_sent;
          ilog("Received a hello_message from peer, sending reply to accept connection");
        }
      }
      else
        FC_THROW("unexpected hello_message from peer");
    }

    void node_impl::on_hello_reply_message(peer_connection* originating_peer, const hello_reply_message& hello_reply_message_received)
    {
      bool already_connected_to_this_peer = is_already_connected_to_id(hello_reply_message_received.node_id);

      // store off the data provided in the hello message
      originating_peer->node_id = hello_reply_message_received.node_id;
      originating_peer->core_protocol_version = hello_reply_message_received.core_protocol_version;
      originating_peer->user_agent = hello_reply_message_received.user_agent;

      if (originating_peer->state == peer_connection::hello_sent && 
          originating_peer->direction == peer_connection_direction::outbound)
      {
        if (already_connected_to_this_peer)
        {
          ilog("Established a connection with peer, but I'm already connected to it.  Closing the connection");
          _closing_connections.insert(originating_peer->shared_from_this());
          _handshaking_connections.erase(originating_peer->shared_from_this());
          originating_peer->close_connection();
        }
        else
        {
          ilog("Received a reply to my \"hello\", connection is accepted");
          ilog("Remote server sees my connection as ${endpoint}", ("endpoint", hello_reply_message_received.remote_endpoint));
          originating_peer->state = peer_connection::connected;
          _active_connections.insert(originating_peer->shared_from_this());
          _handshaking_connections.erase(originating_peer->shared_from_this());
          originating_peer->send_message(address_request_message());
        }
      }
      else
        FC_THROW("unexpected hello_reply_message from peer");
    }

    void node_impl::on_connection_rejected_message(peer_connection* originating_peer, const connection_rejected_message& connection_rejected_message_received)
    {
      if (originating_peer->state == peer_connection::hello_sent && 
          originating_peer->direction == peer_connection_direction::outbound)
      {
        ilog("Received a rejection in response to my \"hello\"");

        // update our database to record that we were rejected so we won't try to connect again for a while
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(originating_peer->get_socket().remote_endpoint());
        updated_peer_record.last_connection_disposition = last_connection_rejected;
        updated_peer_record.last_connection_attempt_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);

        originating_peer->close_connection();
        originating_peer->state = peer_connection::connection_rejected;
        // we could ask the peer for its list of peers now, but for now we'll just go away
        _closing_connections.insert(originating_peer->shared_from_this());
        _handshaking_connections.erase(originating_peer->shared_from_this());
      }
      else
        FC_THROW("unexpected connection_rejected_message from peer");
    }

    void node_impl::on_address_request_message(peer_connection* originating_peer, const address_request_message& address_request_message_received)
    {
      ilog("Received an address request message");
      address_message reply;
      reply.addresses.reserve(_potential_peer_db.size());
      for (auto iter = _potential_peer_db.begin();
           iter != _potential_peer_db.end();
           ++iter)
        reply.addresses.emplace_back(iter->endpoint, iter->last_seen_time);
      originating_peer->send_message(reply);
    }

    void node_impl::on_address_message(peer_connection* originating_peer, const address_message& address_message_received)
    {
      ilog("Received an address message containing ${size} addresses", ("size", address_message_received.addresses.size()));
      for (const address_info& address : address_message_received.addresses)
      {
        ilog("    ${endpoint} last seen ${time}", ("endpoint", address.remote_endpoint)("time", address.last_seen_time));
      }
      bool new_information_received = merge_address_info_with_potential_peer_database(address_message_received.addresses);
      if (new_information_received)
        trigger_p2p_network_connect_loop();
    }

    void node_impl::on_connection_closed(peer_connection* originating_peer)
    {
      peer_connection_ptr originating_peer_ptr = originating_peer->shared_from_this();
      if (_closing_connections.find(originating_peer_ptr) != _closing_connections.end())
        _closing_connections.erase(originating_peer_ptr);
      else if (_active_connections.find(originating_peer_ptr) != _active_connections.end())
        _active_connections.erase(originating_peer_ptr);
      else if (_handshaking_connections.find(originating_peer_ptr) != _handshaking_connections.end())
        _handshaking_connections.erase(originating_peer_ptr);
      ilog("Remote peer ${endpoint} closed their connection to us", ("endpoint", originating_peer->get_remote_endpoint()));
      display_current_connections();
      trigger_p2p_network_connect_loop();
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
          _handshaking_connections.insert(new_peer);

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


    void node_impl::connect_to_task(peer_connection_ptr new_peer, const fc::ip::endpoint& remote_endpoint)
    {
      // create or find the database entry for the new peer
      potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(remote_endpoint);
      updated_peer_record.last_connection_disposition = last_connection_failed;
      updated_peer_record.last_connection_attempt_time = fc::time_point::now();;
      _potential_peer_db.update_entry(updated_peer_record);

      try
      {
        new_peer->connect_to(remote_endpoint/* ,  _node_configuration.listen_endpoint */);  // blocks until the connection is established and secure connection is negotiated

        // connection succeeded.  record that in our database
        updated_peer_record.last_connection_disposition = last_connection_succeeded;
        updated_peer_record.number_of_successful_connection_attempts++;
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);
      }
      catch (fc::exception& except)
      {
        // connection failed.  record that in our database
        updated_peer_record.last_connection_disposition = last_connection_failed;
        updated_peer_record.number_of_failed_connection_attempts++;
        _potential_peer_db.update_entry(updated_peer_record);

        _handshaking_connections.erase(new_peer);
        display_current_connections();
        trigger_p2p_network_connect_loop();

        throw except;
      }
      hello_message hello(_user_agent_string, core_protocol_version, _node_configuration.listen_endpoint, _node_id);
      new_peer->send_message(message(hello));
      new_peer->state = peer_connection::hello_sent;
      ilog("Sent \"hello\" to remote peer");
    }

    // methods implementing node's public interface
    void node_impl::set_delegate(node_delegate* del)
    {
      _delegate = del;
    }

    void node_impl::load_configuration(const fc::path& configuration_directory)
    {
      _node_configuration_directory = configuration_directory;
      fc::path configuration_file_name(_node_configuration_directory / NODE_CONFIGURATION_FILENAME);
      if (fc::exists(configuration_file_name))
      {
        try
        {
          _node_configuration = fc::json::from_file(configuration_file_name).as<detail::node_configuration>();
          ilog("Loaded configuration from file ${filename}", ("filename", configuration_file_name));
        }
        catch (fc::parse_error_exception& parse_error)
        {
          elog("malformed node configuration file ${filename}: ${error}", ("filename", configuration_file_name)("error", parse_error.to_detail_string()));
          throw;
        }
        catch (fc::exception& except)
        {
          elog("unexpected exception while reading configuration file ${filename}: ${error}", ("filename", configuration_file_name)("error", except.to_detail_string()));
          throw;
        }
      }
      fc::path potential_peer_database_file_name(_node_configuration_directory / POTENTIAL_PEER_DATABASE_FILENAME);
      try
      {
        _potential_peer_db.open(potential_peer_database_file_name);
        trigger_p2p_network_connect_loop();
      }
      catch (fc::exception& except)
      {
        elog("unable to open peer database ${filename}: ${error}", ("filename", potential_peer_database_file_name)("error", except.to_detail_string()));
        throw;
      }
    }

    void node_impl::connect_to_p2p_network()
    {
      _p2p_network_connect_loop_done = fc::async([=]() { p2p_network_connect_loop(); });

      if (!_accept_loop_complete.valid())
      {
        try
        {
          if (_node_configuration.listen_endpoint.get_address() != fc::ip::address())
            _tcp_server.listen(_node_configuration.listen_endpoint);
          else
            _tcp_server.listen(_node_configuration.listen_endpoint.port());
          ilog("listening for connections on endpoint ${endpoint}", ("endpoint", _node_configuration.listen_endpoint));
          _accept_loop_complete = fc::async( [=](){ accept_loop(); });
        } FC_RETHROW_EXCEPTIONS(warn, "unable to listen on ${endpoint}", ("endpoint",_node_configuration.listen_endpoint))
      } 
    }

    void node_impl::add_node(const fc::ip::endpoint& ep)
    {
      // TODO
    }

    void node_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      if (is_connection_to_endpoint_in_progress(remote_endpoint))
        FC_THROW("already connected to requested endpoint ${endpoint}", ("endpoint", remote_endpoint));

      ilog("node_impl::connect_to(${endpoint})", ("endpoint", remote_endpoint));
      peer_connection_ptr new_peer(std::make_shared<peer_connection>(std::ref(*this)));
      new_peer->set_remote_endpoint(remote_endpoint);
      _handshaking_connections.insert(new_peer);
      fc::async([=](){ connect_to_task(new_peer, remote_endpoint); });
    }

    bool node_impl::is_connection_to_endpoint_in_progress(const fc::ip::endpoint& remote_endpoint)
    {
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_this_peer(active_peer->get_remote_endpoint());
        if (endpoint_for_this_peer && *endpoint_for_this_peer == remote_endpoint)
          return true;
      }
      for (const peer_connection_ptr& active_peer : _handshaking_connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_this_peer(active_peer->get_remote_endpoint());
        if (endpoint_for_this_peer && *endpoint_for_this_peer == remote_endpoint)
          return true;
      }
      return false;
    }

    void node_impl::listen_on_endpoint(const fc::ip::endpoint& ep)
    {
      _node_configuration.listen_endpoint = ep;
      save_node_configuration();
    }

    void node_impl::listen_on_port(uint16_t port)
    {
      _node_configuration.listen_endpoint = fc::ip::endpoint(fc::ip::address(), port);
      save_node_configuration();
    }

    std::vector<peer_status> node_impl::get_connected_peers() const
    {
      return std::vector<peer_status>();
    }

    void node_impl::broadcast(const message& item_to_broadcast)
    {
      // TODO
    }

    void node_impl::sync_from(const item_id&)
    {
      // TODO
    }

    bool node_impl::is_connected() const
    {
      // TODO
      return false;
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

  void node::set_delegate(node_delegate* del)
  {
    my->set_delegate(del);
  }

  void node::load_configuration(const fc::path& configuration_directory)
  {
    my->load_configuration(configuration_directory);
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

  void node::listen_on_port(uint16_t port)
  {
    my->listen_on_port(port);
  }

  std::vector<peer_status> node::get_connected_peers() const
  {
    return my->get_connected_peers();
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

} } // end namespace bts::net
