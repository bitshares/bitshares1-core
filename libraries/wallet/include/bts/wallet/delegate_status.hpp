#pragma once
#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace wallet {

      enum trust_state
      {
         untrusted,  ///< no user provided trust information
         trusted,    ///< user has indicated trust
         distrusted, ///< user has indiciated distrust
         fired       ///< delegate has been fired for a provable offense   
      };
   struct delegate_status : public bts::blockchain::name_record
   {
      delegate_status( const bts::blockchain::name_record& r )
      :bts::blockchain::name_record( r ){}

      delegate_status()
      :trust_state(untrusted),
       blocks_produced(0),
       blocks_missed(0),
       transactions_included(0),
       transactions_excluded(0){}
      
      fc::enum_type<trust_state,uint8_t> trust_state;
      uint32_t                           blocks_produced;
      uint32_t                           blocks_missed;
      uint32_t                           transactions_included;
      uint32_t                           transactions_excluded;
   };

} }

FC_REFLECT_ENUM( bts::wallet::trust_state, (untrusted)(trusted)(distrusted)(fired) )
FC_REFLECT( bts::wallet::delegate_status, 
            (trust_state)
            (blocks_produced)
            (blocks_missed)
            (transactions_included)
            (transactions_excluded) )
