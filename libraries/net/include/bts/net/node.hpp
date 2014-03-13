#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <bts/net/message.hpp>

namespace bts { namespace net {

   namespace detail { class node_impl; }

   typedef fc::ripemd160 item_id;

   class node_delegate
   {
      public:
         virtual ~node_delegate(){}

         /**
          *  @brief allows the application to validate a message prior to 
          *         broadcasting to peers.
          *
          *  @throws exception if error validating message, otherwise the message is
          *          safe to broadcast on.
          */
         virtual void validate_broadcast( const message& ){};

         /**
          *  Assuming all data elements are ordered in some way, this method should
          *  return up to limit ids that occur *after* from_id.
          */
         virtual std::vector<item_id> get_headers( const item_id& from_id, uint32_t limit = 2000 ){};

         /**
          *  Given the hash of the requested data, fetch the body. 
          */
         virtual message  get_body( const item_id& id ){}; 
   };

   class node
   {
      public:
        node();
        ~node();

        void      set_delegate( node_delegate* del );

        void      broadcast( const message& msg );
        void      sync_from( const data_id& );
        message   fetch( const data_id& );

      private:
        std::unique_ptr<detail::node_impl> my;
   };

} } // bts::net
