#pragma once

#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   struct snapshot_record
   {
      snapshot_record(){}

      snapshot_record( const string& a, share_type b )
      :original_address(a),original_balance(b){}

      string        original_address;
      share_type    original_balance = 0;
   };
   typedef fc::optional<snapshot_record> osnapshot_record;

   struct balance_record
   {
      balance_record(){}

      balance_record( const withdraw_condition& c )
      :condition(c){}

      balance_record( const address& owner, const asset& balance, slate_id_type delegate_id );

      /** condition.get_address() */
      balance_id_type            id()const { return condition.get_address(); }
      asset                      get_spendable_balance( const time_point_sec& at_time )const;
      bool                       is_null()const    { return balance == 0; }
      balance_record             make_null()const  { balance_record cpy(*this); cpy.balance = 0; return cpy; }
      asset_id_type              asset_id()const { return condition.asset_id; }
      slate_id_type              delegate_slate_id()const { return condition.delegate_slate_id; }
      asset                      calculate_yield( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const;

      /** if condition is signature or by name, return the owner */
      address                    owner()const;

      withdraw_condition         condition;
      share_type                 balance = 0;
      osnapshot_record           snapshot_info;
      fc::time_point_sec         deposit_date;
      fc::time_point_sec         last_update;
   };
   typedef fc::optional<balance_record> obalance_record;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::snapshot_record, (original_address)(original_balance) )
FC_REFLECT( bts::blockchain::balance_record, (condition)(balance)(snapshot_info)(deposit_date)(last_update) )
