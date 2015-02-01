#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

   balance_record::balance_record( const address& owner, const asset& balance_arg, slate_id_type delegate_id )
   {
       balance = balance_arg.amount;
       condition = withdraw_condition( withdraw_with_signature( owner ), balance_arg.asset_id, delegate_id );
   }

   set<address> balance_record::owners()const
   {
       return condition.owners();
   }

   optional<address> balance_record::owner()const
   {
       return condition.owner();
   }

   bool balance_record::is_owner( const address& addr )const
   {
       return this->owners().count( addr ) > 0;
   }

   bool balance_record::is_owner( const public_key_type& key )const
   {
       const auto& addrs = this->owners();
       for( const auto& addr : addrs )
       {
           if( addr == address( key ) ) return true;
           if( addr == address( pts_address( key, false, 56 ) ) ) return true;
           if( addr == address( pts_address( key, true, 56 ) ) ) return true;
           if( addr == address( pts_address( key, false, 0 ) ) ) return true;
           if( addr == address( pts_address( key, true, 0 ) ) ) return true;
       }
       return false;
   }

   asset balance_record::get_spendable_balance( const time_point_sec at_time )const
   {
       switch( withdraw_condition_types( condition.type ) )
       {
           case withdraw_signature_type:
           case withdraw_escrow_type:
           case withdraw_multisig_type:
           {
               return asset( balance, condition.asset_id );
           }
           case withdraw_vesting_type:
           {
               const withdraw_vesting vesting_condition = condition.as<withdraw_vesting>();

               // First calculate max that could be claimed assuming no prior withdrawals
               share_type max_claimable = 0;
               if( at_time >= vesting_condition.start_time + vesting_condition.duration )
               {
                   max_claimable = vesting_condition.original_balance;
               }
               else if( at_time > vesting_condition.start_time )
               {
                   const uint32_t elapsed_time = (at_time - vesting_condition.start_time).to_seconds();
                   FC_ASSERT( elapsed_time > 0 && elapsed_time < vesting_condition.duration );
                   const fc::uint128 numerator = fc::uint128( vesting_condition.original_balance ) * fc::uint128( elapsed_time );
                   max_claimable = (numerator / fc::uint128( vesting_condition.duration )).to_uint64();
                   FC_ASSERT( max_claimable >= 0 && max_claimable < vesting_condition.original_balance );
               }

               const share_type claimed_so_far = vesting_condition.original_balance - balance;
               FC_ASSERT( claimed_so_far >= 0 && claimed_so_far <= max_claimable );

               const share_type spendable_balance = max_claimable - claimed_so_far;
               FC_ASSERT( spendable_balance >= 0 && spendable_balance <= max_claimable );
               FC_ASSERT( spendable_balance + claimed_so_far == max_claimable );

               return asset( spendable_balance, condition.asset_id );
           }
           default:
           {
               elog( "balance_record::get_spendable_balance() called on unsupported withdraw type!" );
               return asset();
           }
       }
       FC_ASSERT( false, "Should never get here!" );
   }

   asset balance_record::get_spendable_balance_v1( const time_point_sec at_time )const
   {
       switch( withdraw_condition_types( condition.type ) )
       {
           case withdraw_signature_type:
           case withdraw_escrow_type:
           case withdraw_multisig_type:
           {
               return asset( balance, condition.asset_id );
           }
           case withdraw_vesting_type:
           {
               const withdraw_vesting vesting_condition = condition.as<withdraw_vesting>();

               // First calculate max that could be claimed assuming no prior withdrawals
               share_type max_claimable = 0;
               if( at_time >= vesting_condition.start_time + vesting_condition.duration )
               {
                   max_claimable = vesting_condition.original_balance;
               }
               else if( at_time > vesting_condition.start_time )
               {
                   const auto elapsed_time = (at_time - vesting_condition.start_time).to_seconds();
                   FC_ASSERT( elapsed_time > 0 && elapsed_time < vesting_condition.duration );
                   max_claimable = (vesting_condition.original_balance * elapsed_time) / vesting_condition.duration;
                   FC_ASSERT( max_claimable >= 0 && max_claimable < vesting_condition.original_balance );
               }

               const share_type claimed_so_far = vesting_condition.original_balance - balance;
               FC_ASSERT( claimed_so_far >= 0 && claimed_so_far <= vesting_condition.original_balance );

               const share_type spendable_balance = max_claimable - claimed_so_far;
               FC_ASSERT( spendable_balance >= 0 && spendable_balance <= vesting_condition.original_balance );

               return asset( spendable_balance, condition.asset_id );
           }
           default:
           {
               elog( "balance_record::get_spendable_balance() called on unsupported withdraw type!" );
               return asset();
           }
       }
       FC_ASSERT( false, "Should never get here!" );
   }

    balance_id_type balance_record::get_multisig_balance_id( asset_id_type asset_id, uint32_t m, const vector<address>& addrs )
    { try {
        withdraw_with_multisig multisig_condition;
        multisig_condition.required = m;
        multisig_condition.owners = set<address>(addrs.begin(), addrs.end());
        withdraw_condition condition( multisig_condition, asset_id, 0 );
        auto balance = balance_record(condition);
        return balance.id();
    } FC_CAPTURE_AND_RETHROW( (m)(addrs) ) }

    const balance_db_interface& balance_record::db_interface( const chain_interface& db )
    { try {
        return db._balance_db_interface;
    } FC_CAPTURE_AND_RETHROW() }

    obalance_record balance_db_interface::lookup( const balance_id_type& id )const
    { try {
        return lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    void balance_db_interface::store( const balance_id_type& id, const balance_record& record )const
    { try {
        FC_ASSERT( record.balance >= 0 ); // Sanity check
        insert_into_id_map( id, record );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void balance_db_interface::remove( const balance_id_type& id )const
    { try {
        const obalance_record prev_record = lookup( id );
        if( prev_record.valid() )
            erase_from_id_map( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
