#pragma once
#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace btsx {
   using namespace bts::blockchain;

   namespace detail { class btsx_db_impl; }

   class btsx_db : public chain_database
   {
       public:
          btsx_db();
          ~btsx_db();
    
          void  open( const fc::path& dir, bool create );
          void  close();
    
       private:
          std::unique_ptr<detail::btsx_db_impl> my;
   };


} } // bts::btsx
