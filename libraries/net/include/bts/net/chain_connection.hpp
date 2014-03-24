#pragma once
#include <bts/net/stcp_socket.hpp>
#include <bts/net/message.hpp>
#include <fc/exception/exception.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/db/level_map.hpp>

using namespace bts::blockchain;
using namespace bts::net;

namespace bts { namespace net {
  
   namespace detail { class chain_connection_impl; }

   class chain_connection;
   typedef std::shared_ptr<chain_connection> chain_connection_ptr;

   /** 
    * @brief defines callback interface for chain_connections
    */
   class chain_connection_delegate
   {
      public:
        virtual ~chain_connection_delegate(){}; 
        virtual void on_connection_message( chain_connection& c, const message& m ){};
        virtual void on_connection_disconnected( chain_connection& c ){}
   };



   /**
    *  Manages a connection to a remote p2p node. A connection
    *  processes a stream of messages that have a common header 
    *  and ensures everything is properly encrypted.
    *
    *  A connection also allows arbitrary data to be attached to it
    *  for use by other protocols built at higher levels.
    */
   class chain_connection : public std::enable_shared_from_this<chain_connection>
   {
      public:
        chain_connection( const stcp_socket_ptr& c, chain_connection_delegate* d);
        chain_connection( chain_connection_delegate* d );
        ~chain_connection();

        stcp_socket_ptr  get_socket()const;
        fc::ip::endpoint remote_endpoint()const;
        
        void send( const message& m );

        void connect( const std::string& host_port );  
        void connect( const fc::ip::endpoint& ep );
        void close();

        bts::blockchain::block_id_type get_last_block_id()const;
        void                           set_last_block_id( const bts::blockchain::block_id_type& t );

        void exec_sync_loop();
        void set_database( bts::blockchain::chain_database*  );

      private:
        std::unique_ptr<detail::chain_connection_impl> my;
   };

} } // bts::net   
