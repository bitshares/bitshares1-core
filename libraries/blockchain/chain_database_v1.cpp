#include <bts/blockchain/chain_database_impl.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/market_engine_v1.hpp>
#include <bts/blockchain/market_engine_v2.hpp>
#include <bts/blockchain/market_engine_v3.hpp>
#include <bts/blockchain/market_engine_v4.hpp>
#include <bts/blockchain/market_engine_v5.hpp>
#include <bts/blockchain/market_engine_v6.hpp>
#include <bts/blockchain/market_engine_v7.hpp>
#include <bts/blockchain/market_engine.hpp>

namespace bts { namespace blockchain { namespace detail {

void chain_database_impl::pay_delegate_v1( const block_id_type& block_id,
                                           const public_key_type& block_signee,
                                           const pending_chain_state_ptr& pending_state,
                                           oblock_record& record )const
{ try {
      oaccount_record delegate_record = self->get_account_record( address( block_signee ) );
      FC_ASSERT( delegate_record.valid() );
      delegate_record = pending_state->get_account_record( delegate_record->id );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() && delegate_record->delegate_info.valid() );

      const uint8_t pay_rate_percent = delegate_record->delegate_info->pay_rate;
      FC_ASSERT( pay_rate_percent >= 0 && pay_rate_percent <= 100 );

      const share_type max_available_paycheck = pending_state->get_delegate_pay_rate_v1();
      const share_type accepted_paycheck = (max_available_paycheck * pay_rate_percent) / 100;
      const share_type burned_paycheck = max_available_paycheck - accepted_paycheck;
      FC_ASSERT( max_available_paycheck >= accepted_paycheck );
      FC_ASSERT( accepted_paycheck >= 0 );

      oasset_record base_asset_record = pending_state->get_asset_record( asset_id_type( 0 ) );
      FC_ASSERT( base_asset_record.valid() );
      base_asset_record->current_supply -= burned_paycheck;
      if( pending_state->get_head_block_num() >= BTS_V0_4_16_FORK_BLOCK_NUM )
      {
          base_asset_record->collected_fees -= max_available_paycheck;
          delegate_record->delegate_info->total_burned += burned_paycheck;
      }
      else
      {
          base_asset_record->collected_fees -= accepted_paycheck;
      }
      pending_state->store_asset_record( *base_asset_record );

      delegate_record->delegate_info->votes_for += accepted_paycheck;
      delegate_record->delegate_info->pay_balance += accepted_paycheck;
      delegate_record->delegate_info->total_paid += accepted_paycheck;
      pending_state->store_account_record( *delegate_record );

      if( record.valid() )
      {
          record->signee_shares_issued = 0;
          record->signee_fees_collected = accepted_paycheck;
          record->signee_fees_destroyed = burned_paycheck;
      }
} FC_CAPTURE_AND_RETHROW( (block_id)(block_signee)(record) ) }

void chain_database_impl::execute_markets_v1( const fc::time_point_sec timestamp,
                                              const pending_chain_state_ptr& pending_state )const
{ try {
  vector<market_transaction> market_transactions;

  const auto pending_block_num = pending_state->get_head_block_num();
  if( pending_block_num == BTS_V0_8_0_FORK_BLOCK_NUM )
  {
     market_engine_v7 engine( pending_state, *this );
     engine.cancel_high_apr_shorts();
     market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
  }
  else if( pending_block_num == BTS_V0_4_21_FORK_BLOCK_NUM )
  {
     market_engine_v6 engine( pending_state, *this );
     engine.cancel_all_shorts();
     market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
  }
  else if( pending_block_num == BTS_V0_4_17_FORK_BLOCK_NUM )
  {
     market_engine_v4 engine( pending_state, *this );
     engine.cancel_all_shorts( self->get_block_header( BTS_V0_4_16_FORK_BLOCK_NUM ).timestamp );
     market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
  }
  else if( pending_block_num == BTS_V0_4_16_FORK_BLOCK_NUM )
  {
     // Should have canceled all shorts but we missed it
  }
  else
  {
      const auto dirty_markets = self->get_dirty_markets();
      for( const auto& market_pair : dirty_markets )
      {
         FC_ASSERT( market_pair.first > market_pair.second );
         if( pending_block_num >= BTS_V0_4_21_FORK_BLOCK_NUM )
         {
            market_engine_v7 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else if( pending_block_num >= BTS_V0_4_19_FORK_BLOCK_NUM )
         {
            market_engine_v6 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else if( pending_block_num >= BTS_V0_4_17_FORK_BLOCK_NUM )
         {
            market_engine_v5 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else if( pending_block_num >= BTS_V0_4_16_FORK_BLOCK_NUM )
         {
            market_engine_v4 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else if( pending_block_num >= BTS_V0_4_13_FORK_BLOCK_NUM )
         {
            market_engine_v3 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else if( pending_block_num >= BTS_V0_4_0_FORK_BLOCK_NUM )
         {
            market_engine_v2 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
         else
         {
            market_engine_v1 engine( pending_state, *this );
            if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
            {
               market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
            }
         }
      }
  }

  if( pending_block_num < BTS_V0_4_9_FORK_BLOCK_NUM )
      pending_state->set_dirty_markets( pending_state->_dirty_markets );

  pending_state->set_market_transactions( std::move( market_transactions ) );
} FC_CAPTURE_AND_RETHROW( (timestamp) ) }

void chain_database_impl::update_active_delegate_list_v1( const uint32_t block_num,
                                                          const pending_chain_state_ptr& pending_state )const
{ try {
    if( block_num % BTS_BLOCKCHAIN_NUM_DELEGATES != 0 )
        return;

    auto active_del = self->next_round_active_delegates();
    const size_t num_del = active_del.size();

    // Perform a random shuffle of the sorted delegate list.
    fc::sha256 rand_seed = fc::sha256::hash( pending_state->get_current_random_seed() );
    for( uint32_t i = 0; i < num_del; ++i )
    {
        for( uint32_t x = 0; x < 4 && i < num_del; ++x, ++i )
            std::swap( active_del[ i ], active_del[ rand_seed._hash[ x ] % num_del ] );

        rand_seed = fc::sha256::hash( rand_seed );
    }

    pending_state->set_active_delegates( active_del );
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

} } } // bts::blockchain::detail
