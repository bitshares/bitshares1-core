#pragma once
#include <bts/net/stcp_socket.hpp>
#include <bts/net/message.hpp>
#include <fc/exception/exception.hpp>
#include <bts/db/level_map.hpp>

namespace bts { namespace net {
  
   namespace detail { class connection_impl; }

   class connection;
   struct message;
   struct message_header;

   typedef std::shared_ptr<connection> connection_ptr;

   /** 
    * @brief defines callback interface for connections
    */
   class connection_delegate
   {
      public:
        /** Called when given network connection has completed receiving a message and it is ready
            for further processing.
        */
        virtual void on_connection_message( connection& c, const message& m ) = 0;
        /// Called when connection has been lost.
        virtual void on_connection_disconnected( connection& c ) = 0;

      protected:
        /// Only implementation is responsible for this object lifetime.
        virtual ~connection_delegate() {}
   };



   /**
    *  Manages a connection to a remote p2p node. A connection
    *  processes a stream of messages that have a common header 
    *  and ensures everything is properly encrypted.
    *
    *  A connection also allows arbitrary data to be attached to it
    *  for use by other protocols built at higher levels.
    */
   class connection : public std::enable_shared_from_this<connection>
   {
      public:
        connection( const stcp_socket_ptr& c, connection_delegate* d);
        connection( connection_delegate* d );
        ~connection();
   
        stcp_socket_ptr  get_socket()const;
        fc::ip::endpoint remote_endpoint()const;
        
        void send( const message& m );
   
        void connect( const std::string& host_port );  
        void connect( const fc::ip::endpoint& ep );
        void close();

        void exec_sync_loop();

      private:
        std::unique_ptr<detail::connection_impl> my;
   };

    
} } // bts:net
