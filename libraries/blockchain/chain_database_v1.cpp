#include <bts/blockchain/chain_database_impl.hpp>
#include <bts/blockchain/fork_blocks.hpp>

using namespace bts::blockchain;
using namespace bts::blockchain::detail;

void chain_database_impl::pay_delegate( const block_id_type& block_id,
                                        const pending_chain_state_ptr& pending_state,
                                        const public_key_type& block_signee )
{ try {
      auto delegate_record = pending_state->get_account_record( self->get_delegate_record_for_signee( block_signee ).id );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

      const auto pay_rate_percent = delegate_record->delegate_info->pay_rate;
      FC_ASSERT( pay_rate_percent >= 0 && pay_rate_percent <= 100 );
      const auto max_available_paycheck = pending_state->get_delegate_pay_rate();
      const auto accepted_paycheck = ( pay_rate_percent * max_available_paycheck ) / 100;

      auto pending_base_record = pending_state->get_asset_record( asset_id_type( 0 ) );
      FC_ASSERT( pending_base_record.valid() );
      if( pending_state->get_head_block_num() >= BTSX_SUPPLY_FORK_1_BLOCK_NUM )
      {
          pending_base_record->collected_fees -= max_available_paycheck;
      }
      else
      {
          pending_base_record->collected_fees -= accepted_paycheck;
      }
      pending_state->store_asset_record( *pending_base_record );

      delegate_record->delegate_info->pay_balance += accepted_paycheck;
      delegate_record->delegate_info->votes_for += accepted_paycheck;
      pending_state->store_account_record( *delegate_record );

      auto base_asset_record = pending_state->get_asset_record( asset_id_type(0) );
      FC_ASSERT( base_asset_record.valid() );
      base_asset_record->current_share_supply -= (max_available_paycheck - accepted_paycheck);
      pending_state->store_asset_record( *base_asset_record );
} FC_CAPTURE_AND_RETHROW( (block_id)(block_signee) ) }
