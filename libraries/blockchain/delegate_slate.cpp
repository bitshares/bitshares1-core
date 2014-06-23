#include <bts/blockchain/delegate_slate.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/io/raw.hpp>
#include <algorithm>

namespace bts { namespace blockchain {

   uint64_t delegate_slate::id()const
   {
     // std::sort( supported_delegates.begin(), supported_delegates.end() );
     // std::sort( rejected_delegates.begin(), rejected_delegates.end() );
      if( supported_delegates.size() == 0 )
         return 0;
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result()._hash[0];
   }

} }
