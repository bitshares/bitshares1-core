#include <bts/blockchain/chain_database_impl.hpp>
#include <bts/blockchain/fork_blocks.hpp>

using namespace bts::blockchain;
using namespace bts::blockchain::detail;

void chain_database_impl::pay_delegate_v1( const block_id_type& block_id,
                                           const pending_chain_state_ptr& pending_state,
                                           const public_key_type& block_signee )
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
      base_asset_record->current_share_supply -= burned_paycheck;
      if( pending_state->get_head_block_num() >= BTSX_SUPPLY_FORK_1_BLOCK_NUM )
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
} FC_CAPTURE_AND_RETHROW( (block_id)(block_signee) ) }
