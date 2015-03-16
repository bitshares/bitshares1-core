#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
};

// Initialized in load_checkpoints()
static uint32_t LAST_CHECKPOINT_BLOCK_NUM = 0;

} } // bts::blockchain
