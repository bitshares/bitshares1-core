#pragma once
#include <bts/net/core_messages.hpp>
#include <bts/net/message.hpp>

namespace bts { namespace net {

   namespace detail { class node_impl; }

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
         virtual bool has_item( const net::item_id& id ) = 0;

         /**
          *  @brief allows the application to validate an item prior to 
          *         broadcasting to peers.
          *
          *  @throws exception if error validating the item, otherwise the item is
          *          safe to broadcast on.
          */
         virtual void handle_message( const message& ) = 0;

         /**
          *  Assuming all data elements are ordered in some way, this method should
          *  return up to limit ids that occur *after* from_id.
          *  On return, remaining_item_count will be set to the number of items
          *  in our blockchain after the last item returned in the result,
          *  or 0 if the result contains the last item in the blockchain
          */
         virtual std::vector<item_hash_t> get_item_ids( const item_id& from_id, 
                                                        uint32_t& remaining_item_count,
                                                        uint32_t limit = 2000 ) = 0;

         /**
          *  Given the hash of the requested data, fetch the body. 
          */
         virtual message get_item( const item_id& id ) = 0;

         /**
          *  Call this after the call to handle_message succeeds.
          * 
          *  @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
          *  @param item_count the number of items known to the node that haven't been sent to handle_item() yet.
          *                    After `item_count` more calls to handle_item(), the node will be in sync
          */
         virtual void     sync_status( uint32_t item_type, uint32_t item_count ) = 0;

         /**
          *  Call any time the number of connected peers changes.
          */
         virtual void     connection_count_changed( uint32_t c ) = 0;
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
    *
    *  Unanswered questions:
    *    when does the node start establishing network connections and accepting peers?
    *    we don't have enough info to start synchronizing until sync_from() is called,
    *    would we have any reason to connect before that?
    */
   class node
   {
      public:
        node();
        ~node();

        void      set_delegate( node_delegate* del );

        void      load_configuration( const fc::path& configuration_directory );

        void      connect_to_p2p_network();

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
         *  Specifies the port upon which incoming connections should be accepted.
         */
        void      listen_on_port(uint16_t port);

        /**
         *  @return a list of peers that are currently connected.
         */
        std::vector<peer_status> get_connected_peers()const;

        /**
         *  Add message to outgoing inventory list, notify peers that
         *  I have a message ready.
         */
        void      broadcast( const message& item_to_broadcast );

        /**
         *  Node starts the process of fetching all items after item_id of the
         *  given item_type.   During this process messages are not broadcast.
         */
        void      sync_from( const item_id& );

        bool      is_connected()const;

      private:
        std::unique_ptr<detail::node_impl> my;
   };

   typedef std::shared_ptr<node> node_ptr;

} } // bts::net
