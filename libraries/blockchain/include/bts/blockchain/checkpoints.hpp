#pragma once

#include <map>

#include <bts/blockchain/types.hpp>

const static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS {
    {1, bts::blockchain::block_id_type("8abcfb93c52f999e3ef5288c4f837f4f15af5521")},
    {225000, bts::blockchain::block_id_type("2e09195c3e4ef6d58736151ea22f78f08556e6a9")},
    {282000, bts::blockchain::block_id_type("611197d4a3f90f5c9009559b896f0f5d4a4f4b5f")},
    {312818, bts::blockchain::block_id_type("8d1cb83d5d98bebbef06820f627fc00bd33c8cdc")}
};
