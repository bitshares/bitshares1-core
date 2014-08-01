#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {
   void update_feed_operation::evaluate( transaction_evaluation_state& eval_state )
   {
      FC_ASSERT( eval_state._current_state->is_active_delegate( feed.delegate_id ) );
      FC_ASSERT( eval_state.check_signature( eval_state._current_state->get_account_record( feed.delegate_id )->active_key() ) );
      auto now = eval_state._current_state->now();
      eval_state._current_state->set_feed( feed_record{ feed, value, now } );
   }
} }  // bts::blockchain
