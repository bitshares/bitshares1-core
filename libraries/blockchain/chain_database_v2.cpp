#include <bts/blockchain/chain_database_impl.hpp>
#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain { namespace detail {

void chain_database_impl::pay_delegate_v2( const block_id_type& block_id,
                                           const public_key_type& block_signee,
                                           const pending_chain_state_ptr& pending_state,
                                           oblock_record& record )const
{ try {
      if( pending_state->get_head_block_num() < BTS_V0_4_24_FORK_BLOCK_NUM )
          return pay_delegate_v1( block_id, block_signee, pending_state, record );

      oaccount_record delegate_record = self->get_account_record( address( block_signee ) );
      FC_ASSERT( delegate_record.valid() );
      delegate_record = pending_state->get_account_record( delegate_record->id );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() && delegate_record->delegate_info.valid() );

      const uint8_t pay_rate_percent = delegate_record->delegate_info->pay_rate;
      FC_ASSERT( pay_rate_percent >= 0 && pay_rate_percent <= 100 );
      const share_type accepted_paycheck = (self->get_max_delegate_pay_issued_per_block() * pay_rate_percent) / 100;
      FC_ASSERT( accepted_paycheck >= 0 );

      delegate_record->delegate_info->votes_for += accepted_paycheck;
      delegate_record->delegate_info->pay_balance += accepted_paycheck;
      delegate_record->delegate_info->total_paid += accepted_paycheck;
      pending_state->store_account_record( *delegate_record );

      oasset_record base_asset_record = pending_state->get_asset_record( asset_id_type( 0 ) );
      FC_ASSERT( base_asset_record.valid() );
      const share_type destroyed_collected_fees = base_asset_record->collected_fees;
      base_asset_record->current_supply += accepted_paycheck;
      base_asset_record->current_supply -= destroyed_collected_fees;
      base_asset_record->collected_fees = 0;
      pending_state->store_asset_record( *base_asset_record );

      if( record.valid() )
      {
          record->signee_shares_issued = accepted_paycheck;
          record->signee_fees_collected = 0;
          record->signee_fees_destroyed = destroyed_collected_fees;
      }
} FC_CAPTURE_AND_RETHROW( (block_id)(block_signee)(record) ) }

} } } // bts::blockchain::detail
