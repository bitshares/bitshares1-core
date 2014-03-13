#pragma once

namespace bts { namespace net {

   namespace detail { class node_impl; }

   typedef std::vector<char> data_type;
   typedef fc::ripemd160     data_id;

   class node_delegate
   {
      public:
         virtual ~node_delegate(){}

         /**
          *  @brief allows the application to validate a message prior to 
          *         broadcasting to peers.
          */
         virtual void validate_broadcast( const data_type& ){};

         /**
          *  Assuming all data elements are ordered in some way, this method should
          *  return up to limit ids that occur *after* from_id.
          */
         virtual std::vector<data_id> get_headers( const data_id& from_id, uint32_t limit = 2000 ){};

         /**
          *  Given the hash of the requested data, fetch the body. 
          */
         virtual data_type  get_body( const data_id& id ){}; 
   };

   class node
   {
      public:
        node();
        ~node();

        void      set_delegate( node_delegate* del );

        void      broadcast( const data& msg );
        void      sync_from( const data_id& );
        data_type fetch( const data_id& );

      private:
        std::unique_ptr<detail::node_impl> my;
   };

} } // bts::net
