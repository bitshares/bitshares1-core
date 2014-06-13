#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   /**
    */
   struct balance_record
   {
      balance_record():balance(0){}
      balance_record( const withdraw_condition& c )
      :balance(0),condition(c){}

      balance_record( const address& owner, const asset& balance, name_id_type delegate_id );

      /** condition.get_address() */
      balance_id_type            id()const { return condition.get_address(); }
      asset                      get_balance()const;
      bool                       is_null()const    { return balance == 0; }
      balance_record             make_null()const  { balance_record cpy(*this); cpy.balance = 0; return cpy; }
      asset_id_type              asset_id()const { return condition.asset_id; }
      account_id_type            delegate_id()const { return condition.delegate_id; }

      /** if conndition is signature or by name, return the owner */
      address                    owner()const;

      share_type                 balance;
      withdraw_condition         condition;
      fc::time_point_sec         last_update;
   };
   typedef fc::optional<balance_record> obalance_record;
} }

FC_REFLECT( bts::blockchain::balance_record, (balance)(condition)(last_update) )
