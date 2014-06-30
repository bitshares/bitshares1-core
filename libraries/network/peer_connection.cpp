#include <bts/network/config.hpp>
#include <bts/network/peer_connection.hpp>
#include <bts/network/exceptions.hpp>

#include <fc/thread/unique_lock.hpp>

namespace bts { namespace network {

   peer_connection::peer_connection()
   {
   }

   peer_connection::~peer_connection()
   {
      try {
         _socket.close();
      } catch ( ... ) {}

      if( _read_loop.valid() )
      {
         try{
         _read_loop.cancel();
         _read_loop.wait();
         } catch ( ... ) {}
      }
      if( _send_queue_complete.valid() )
      {
         try{
         _send_queue_complete.cancel();
         _send_queue_complete.wait();
         } catch ( ... ) {}
      }
   }

   void peer_connection::start_read_loop()
   {
      if( !_read_loop.valid() )
         _read_loop = fc::async( [this](){ read_loop(); } );
   }

   void peer_connection::read_loop()
   {
      ilog( "read loop" );
      try 
      {
         auto one_time_key = fc::ecc::private_key::generate();
         fc::ecc::public_key pub = one_time_key.get_public_key();
         auto s = pub.serialize();

         _socket.write( (char*)&s, sizeof(s) );

         fc::ecc::public_key_data remote_one_time_key;
         _socket.read( (char*)&remote_one_time_key, sizeof(remote_one_time_key) );

         _shared_secret = one_time_key.get_shared_secret( remote_one_time_key );

         elog( "${ss}", ("ss",_shared_secret) );

         if( _send_queue.size() && !_send_queue_complete.valid() )
            _send_queue_complete = fc::async( [=](){ process_send_queue(); } );

         message next_message;
         next_message.data.resize( BTS_NETWORK_MAX_MESSAGE_SIZE );

         while( !_read_loop.canceled() )
         {
            // read a message
            _socket.read( (char*)&next_message.size, sizeof(next_message.size) );
            wlog( "                                                      read message of size ${s} ", ("s", next_message.size) );

            if( next_message.size > BTS_NETWORK_MAX_MESSAGE_SIZE )
            {
               send_message( goodbye_message( message_too_large() ) ); 
               _socket.close();
               FC_CAPTURE_AND_THROW( message_too_large, (next_message.size) );
            }

            _socket.read( (char*)&next_message.type, sizeof(next_message.type) );
            wlog( "                     read message of size ${s}   type ${t}", ("s", next_message.size)("t",int(next_message.type)) );
            _socket.read( next_message.data.data(), next_message.size );
            wlog( "read body of message" );

            received_message( shared_from_this(), next_message );
         }
      } 
      catch ( const fc::exception& e )
      {
         ilog( "closed: ${e}", ("e", e.to_detail_string()) );
         connection_closed( shared_from_this(), e );
         return;
      }
      ilog( "closed!" );
      connection_closed( shared_from_this(), optional<fc::exception>() );
   }

   fc::ip::endpoint peer_connection::remote_endpoint()
   {
      if( !_remote_endpoint )
         _remote_endpoint = _socket.remote_endpoint();
      return *_remote_endpoint;
   }

   bool peer_connection::is_logged_in()const
   {
      return info.delegate_ids.size() > 0;
   }

   void peer_connection::send_message( const message& m )
   { 
         _send_queue.push_back(m);
         if( _shared_secret != fc::sha512() && _send_queue.size() == 1 )
            _send_queue_complete = fc::async( [=](){ process_send_queue(); } );
   }
   void peer_connection::process_send_queue()
   {
      try {
         while( _send_queue.size() )
         {
            message& m = _send_queue.front();
            ilog( "sending message of size ${s}", ("s",m.size) );
            _socket.write( (char*)&m.size, sizeof(m.size) );
            _socket.write( (char*)&m.type, sizeof(m.type) );

            if( m.data.size() >= m.size && m.size )
            {
              _socket.write( (char*)m.data.data(), m.size );
            }
            _send_queue.pop_front();
         }
      } catch ( const fc::exception& e ) {
          wlog( "error sending message ${e}", ("e",e.to_detail_string() ) );
          _socket.close();
          connection_closed( shared_from_this(), e );
      }
   }

   void peer_connection::signin( const fc::ecc::private_key& key )
   {
      auto sig = key.sign_compact( fc::sha256::hash( (char*)&_shared_secret, sizeof(_shared_secret) ) );
      auto self = shared_from_this();
      fc::async( [=](){ self->send_message( signin_request( sig ) ); } );
   }

} }
