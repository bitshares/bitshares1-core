#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <bts/net/message.hpp>

namespace bts { namespace net {

   namespace detail { class node_impl; }

   typedef fc::ripemd160 item_id;

   /**
    *  @class node_delegate
    *  @brief used by node reports status to client or fetch data from client
    */
   class node_delegate
   {
      public:
         virtual ~node_delegate(){}

         /**
          *  If delegate has the item, the network has no need to fetch it.
          */
         virtual bool has_item( const item_id& id );

         /**
          *  @brief allows the application to validate a message prior to 
          *         broadcasting to peers.
          *
          *  @throws exception if error validating message, otherwise the message is
          *          safe to broadcast on.
          */
         virtual void handle_message( const message& ){};

         /**
          *  Assuming all data elements are ordered in some way, this method should
          *  return up to limit ids that occur *after* from_id.
          */
         virtual std::vector<item_id> get_headers( uint32_t item_type, 
                                                   const item_id& from_id, 
                                                   uint32_t limit = 2000 );

         /**
          *  Given the hash of the requested data, fetch the body. 
          */
         virtual message  get_body( uint32_t type, const item_id& id ); 

         /**
          *  Call this after the call to handle_message succeeds.  
          */
         virtual void     sync_status( uint32_t item_type, uint32_t current_item_num, uint32_t item_count );

         /**
          *  Call any time the number of connected peers changes.
          */
         virtual void     connection_count_changed( uint32_t c );
   };

   /**
    *  Information about connected peers that the client may want to make
    *  available to the user.
    */
   struct peer_status
   {
      uint32_t         version;
      fc::ip::endpoint host;
      fc::variant      info;
   };

   /**
    *  @class node
    *  @brief provides application independent P2P broadcast and data synchronization
    */
   class node
   {
      public:
        node();
        ~node();

        void      set_delegate( node_delegate* del );

        /**
         *  Add endpoint to internal level_map database of potential nodes
         *  to attempt to connect to.  This database is consulted any time
         *  the number connected peers falls below the target.
         */
        void      add_node( const fc::ip::endpoint& ep );

        /**
         *  Attempt to connect to the specified endpoint immediately.
         */
        void      connect_to( const fc::ip::endpoint& ep );

        /**
         *  Specifies the network interface and port upon which incoming
         *  connections should be accepted.
         */
        void      listen_on_endpoint( const fc::ip::endpoint& ep );

        /**
         *  @return a list of peers that are currently connected.
         */
        std::vector<peer_status> get_connected_peers()const;

        /**
         *  Add message to outgoing inventory list, notify peers that
         *  I have a message ready.
         */
        void      broadcast( const message& msg );

        /**
         *  Node starts the process of fetching all items after item_id of the
         *  given item_type.   Durring this process messages are not broadcast.
         */
        void      sync_from( uint32_t item_type, const item_id& );

      private:
        std::unique_ptr<detail::node_impl> my;
   };

} } // bts::net
