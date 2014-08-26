#pragma once

#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   struct genesis_record
   {
      genesis_record(){}

      genesis_record( const asset& b, const string& a )
      :initial_balance(b),claim_addr(a){}

      asset     initial_balance;
      string    claim_addr;
   };
   typedef fc::optional<genesis_record> ogenesis_record;

   struct balance_record
   {
      balance_record(){}

      balance_record( const withdraw_condition& c )
      :condition(c){}

      balance_record( const address& owner, const asset& balance, slate_id_type delegate_id );

      /** condition.get_address() */
      balance_id_type            id()const { return condition.get_address(); }
      asset                      get_balance()const;
      bool                       is_null()const    { return balance == 0; }
      balance_record             make_null()const  { balance_record cpy(*this); cpy.balance = 0; return cpy; }
      asset_id_type              asset_id()const { return condition.asset_id; }
      slate_id_type              delegate_slate_id()const { return condition.delegate_slate_id; }

      /** if condition is signature or by name, return the owner */
      address                    owner()const;

      share_type                 balance = share_type( 0 );
      withdraw_condition         condition;
      // TODO: We can do better than this
      ogenesis_record            genesis_info;
      fc::time_point_sec         last_update;
   };
   typedef fc::optional<balance_record> obalance_record;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::genesis_record, (initial_balance)(claim_addr) )
FC_REFLECT( bts::blockchain::balance_record, (balance)(condition)(genesis_info)(last_update) )
