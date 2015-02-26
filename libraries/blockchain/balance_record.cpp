#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

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

    balance_id_type balance_record::get_multisig_balance_id( asset_id_type asset_id, uint32_t m, const vector<address>& addrs )
    { try {
        withdraw_with_multisig multisig_condition;
        multisig_condition.required = m;
        multisig_condition.owners = set<address>(addrs.begin(), addrs.end());
        withdraw_condition condition( multisig_condition, asset_id, 0 );
        const auto balance = balance_record( condition );
        return balance.id();
    } FC_CAPTURE_AND_RETHROW( (m)(addrs) ) }

    void balance_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( balance >= 0 );
        FC_ASSERT( condition.asset_id == 0 || db.lookup<asset_record>( condition.asset_id ).valid() );
        FC_ASSERT( condition.slate_id == 0 || db.lookup<slate_record>( condition.slate_id ).valid() );
        switch( withdraw_condition_types( condition.type ) )
        {
            case withdraw_signature_type:
            case withdraw_vesting_type:
            case withdraw_multisig_type:
            case withdraw_escrow_type:
                break;
            default:
                FC_CAPTURE_AND_THROW( invalid_withdraw_condition );
        }
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    obalance_record balance_record::lookup( const chain_interface& db, const balance_id_type& id )
    { try {
        return db.balance_lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    void balance_record::store( chain_interface& db, const balance_id_type& id, const balance_record& record )
    { try {
        db.balance_insert_into_id_map( id, record );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void balance_record::remove( chain_interface& db, const balance_id_type& id )
    { try {
        const obalance_record prev_record = db.lookup<balance_record>( id );
        if( prev_record.valid() )
            db.balance_erase_from_id_map( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
