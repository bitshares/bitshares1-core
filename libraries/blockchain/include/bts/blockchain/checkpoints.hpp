#pragma once

#include <map>

#include <bts/blockchain/types.hpp>

const static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS {
    {1, bts::blockchain::block_id_type("8abcfb93c52f999e3ef5288c4f837f4f15af5521")},
    {206632, bts::blockchain::block_id_type("565ab0f92e3af4369d11ecd0ea0b95c93eb74a1c")}
};
