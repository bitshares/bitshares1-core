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
         _read_loop.cancel();
         _read_loop.wait();
      }
   }

   void peer_connection::start_read_loop()
   {
      _read_loop = fc::async( [this](){ read_loop(); } );
   }

   void peer_connection::read_loop()
   {
      try 
      {
         auto one_time_key = fc::ecc::private_key::generate();
         fc::ecc::public_key pub = one_time_key.get_public_key();
         auto s = pub.serialize();
         _socket.write( (char*)&s, sizeof(s) );

         fc::ecc::public_key_data remote_one_time_key;
         _socket.read( (char*)&remote_one_time_key, sizeof(remote_one_time_key) );

         _shared_secret = one_time_key.get_shared_secret( remote_one_time_key );

         message next_message;
         next_message.data.resize( BTS_NETWORK_MAX_MESSAGE_SIZE );

         while( !_read_loop.canceled() )
         {
            // read a message
            _socket.read( (char*)&next_message.size, sizeof(next_message.size) );

            if( next_message.size > BTS_NETWORK_MAX_MESSAGE_SIZE )
            {
               send_message( goodbye_message( message_too_large() ) ); 
               _socket.close();
               FC_CAPTURE_AND_THROW( message_too_large, (next_message.size) );
            }

            _socket.read( (char*)&next_message.type, sizeof(next_message.type) );
            _socket.read( next_message.data.data(), next_message.size );
            ilog( "read message of size ${s}   type ${t}", ("s", next_message.size)("t",int(next_message.type)) );

            received_message( shared_from_this(), next_message );
         }
      } 
      catch ( const fc::exception& e )
      {
         connection_closed( shared_from_this(), e );
         return;
      }
      connection_closed( shared_from_this(), optional<fc::exception>() );
   }

   bool peer_connection::is_logged_in()const
   {
      return info.delegate_ids.size() > 0;
   }

   void peer_connection::send_message( const message& m )
   { 
      try {
         { synchronized( _write_mutex )
            ilog( "sending message of size ${s}", ("s",m.size) );
            _socket.write( (char*)&m.size, sizeof(m.size) );
            _socket.write( (char*)&m.type, sizeof(m.type) );

            if( m.data.size() )
            {
              _socket.write( (char*)m.data.data(), sizeof(m.data.size()) );
            }
         }
      }
      catch ( const fc::exception& e )
      {
          wlog( "error sending message ${e}", ("e",e.to_detail_string() ) );
          _socket.close();
      }
   }

   void peer_connection::signin( const fc::ecc::private_key& key )
   {
      auto sig = key.sign_compact( fc::sha256::hash( (char*)&_shared_secret, sizeof(_shared_secret) ) );
      auto self = shared_from_this();
      fc::async( [=](){ self->send_message( signin_request( sig ) ); } );
   }

} }
