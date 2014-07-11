#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

   /**
    *  To support approval voting every deposit must specify the 
    *  slate of delegates its funds are supporting.  This is done as
    *  an array of delegate account IDs.  Delegate accounts are 
    */
   struct delegate_slate
   {
      slate_id_type         id()const;
#if BTS_BLOCKCHAIN_VERSION > 105 
      vector<signed_int>    supported_delegates;
#warning [HARDFORK] Remove below deprecated member
#else
      vector<unsigned_int>  supported_delegates;
#endif
   };
   typedef optional<delegate_slate> odelegate_slate;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::delegate_slate, (supported_delegates) )
