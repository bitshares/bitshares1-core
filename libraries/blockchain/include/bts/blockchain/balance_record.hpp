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

   class chain_interface;
   struct balance_db_interface;
   struct balance_record
   {
      balance_record(){}

      balance_record( const withdraw_condition& c )
      :condition(c){}

      balance_record( const address& owner, const asset& balance, slate_id_type delegate_id );

      balance_id_type            id()const { return condition.get_address(); }
      slate_id_type              slate_id()const { return condition.slate_id; }

      set<address>               owners()const;
      optional<address>          owner()const;
      bool                       is_owner( const address& addr )const;
      bool                       is_owner( const public_key_type& key )const;

      asset_id_type              asset_id()const { return condition.asset_id; }
      asset                      get_spendable_balance( const time_point_sec at_time )const;
      asset                      get_spendable_balance_v1( const time_point_sec at_time )const;
      asset                      calculate_yield( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const;
      asset                      calculate_yield_v1( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const;

      withdraw_condition         condition;
      share_type                 balance = 0;
      optional<address>          restricted_owner;
      osnapshot_record           snapshot_info;
      fc::time_point_sec         deposit_date;
      fc::time_point_sec         last_update;
      variant                    meta_data; // extra meta data about every balance

      static balance_id_type get_multisig_balance_id( asset_id_type asset_id, uint32_t m, const vector<address>& addrs );

      static const balance_db_interface& db_interface( const chain_interface& );
   };
   typedef fc::optional<balance_record> obalance_record;

   struct balance_db_interface
   {
       std::function<obalance_record( const balance_id_type& )>             lookup_by_id;
       std::function<void( const balance_id_type&, const balance_record& )> insert_into_id_map;
       std::function<void( const balance_id_type& )>                        erase_from_id_map;

       obalance_record lookup( const balance_id_type& )const;
       void store( const balance_id_type&, const balance_record& )const;
       void remove( const balance_id_type& )const;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::snapshot_record, (original_address)(original_balance) )
FC_REFLECT( bts::blockchain::balance_record, (condition)(balance)(restricted_owner)(snapshot_info)(deposit_date)(last_update)(meta_data) )
