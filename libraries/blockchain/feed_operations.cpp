#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

   void update_feed_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      const oaccount_record account_record = eval_state._current_state->get_account_record( feed.delegate_id );
      if( !account_record.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (feed.delegate_id) );

      if( account_record->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (*account_record) );

      if( !account_record->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (*account_record) );

      if( !eval_state.check_signature( account_record->delegate_info->block_signing_key )
          && !eval_state.account_or_any_parent_has_signed( *account_record ) )
      {
          FC_CAPTURE_AND_THROW( missing_signature, (*this) );
      }

      eval_state._current_state->set_feed( feed_record{ feed, value, eval_state._current_state->now() } );
      eval_state._current_state->set_market_dirty( feed.feed_id, 0 );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
