#define DEFAULT_LOGGER "p2p"
#include <bts/net/peer_connection.hpp>

namespace bts { namespace net
  {
    fc::tcp_socket& peer_connection::get_socket()
    {
      return _message_connection.get_socket();
    }

    void peer_connection::accept_connection()
    {
      try
      {
        assert( our_state == our_connection_state::disconnected &&
               their_state == their_connection_state::disconnected );
        direction = peer_connection_direction::inbound;
        negotiation_status = connection_negotiation_status::accepting;
        _message_connection.accept();           // perform key exchange
        negotiation_status = connection_negotiation_status::accepted;
        _remote_endpoint = _message_connection.get_socket().remote_endpoint();

        // firewall-detecting info is pretty useless for inbound connections, but initialize 
        // it the best we can
        fc::ip::endpoint local_endpoint = _message_connection.get_socket().local_endpoint();
        inbound_address = local_endpoint.get_address();
        inbound_port = local_endpoint.port();
        outbound_port = inbound_port;

        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog( "established inbound connection from ${remote_endpoint}, sending hello", ("remote_endpoint", _message_connection.get_socket().remote_endpoint() ) );
      }
      catch ( const fc::exception& e )
      {
        wlog( "error accepting connection ${e}", ("e", e.to_detail_string() ) );
        throw;
      }
    }

    void peer_connection::connect_to( const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint )
    {
      try
      {
        assert( our_state == our_connection_state::disconnected && their_state == their_connection_state::disconnected );
        direction = peer_connection_direction::outbound;

        _remote_endpoint = remote_endpoint;
        if( local_endpoint )
        {
          // the caller wants us to bind the local side of this socket to a specific ip/port
          // This depends on the ip/port being unused, and on being able to set the 
          // SO_REUSEADDR/SO_REUSEPORT flags, and either of these might fail, so we need to 
          // detect if this fails.
          try
          {
            _message_connection.bind( *local_endpoint );
          }
          catch ( const fc::exception& except )
          {
            wlog( "Failed to bind to desired local endpoint ${endpoint}, will connect using an OS-selected endpoint: ${except}", ("endpoint", *local_endpoint )("except", except ) );
          }
        }
        negotiation_status = connection_negotiation_status::connecting;
        _message_connection.connect_to( remote_endpoint );
        negotiation_status = connection_negotiation_status::connected;
        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog( "established outbound connection to ${remote_endpoint}", ("remote_endpoint", remote_endpoint ) );
      }
      catch ( fc::exception& e )
      {
        elog( "fatal: error connecting to peer ${remote_endpoint}: ${e}", ("remote_endpoint", remote_endpoint )("e", e.to_detail_string() ) );
        throw;
      }
    } // connect_to()

    void peer_connection::on_message( message_oriented_connection* originating_connection, const message& received_message )
    {
      _node->on_message( this, received_message );
    }

    void peer_connection::on_connection_closed( message_oriented_connection* originating_connection )
    {
      negotiation_status = connection_negotiation_status::closed;
      _node->on_connection_closed( this );
    }

    void peer_connection::send_message( const message& message_to_send )
    {
      _message_connection.send_message( message_to_send );
    }

    void peer_connection::close_connection()
    {
      negotiation_status = connection_negotiation_status::closing;
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

    void peer_connection::set_remote_endpoint( fc::optional<fc::ip::endpoint> new_remote_endpoint )
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

} } // end namespace bts::net
