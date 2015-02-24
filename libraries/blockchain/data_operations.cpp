#include <bts/blockchain/data_operations.hpp>

#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

    void data_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
#ifndef WIN32
#warning [SOFTFORK] Remove this check after BTS_V0_7_0_FORK_BLOCK_NUM has passed
#endif
      FC_ASSERT( eval_state._current_state->get_head_block_num() >= BTS_V0_7_0_FORK_BLOCK_NUM );
    }

}} // bts::blockchain
