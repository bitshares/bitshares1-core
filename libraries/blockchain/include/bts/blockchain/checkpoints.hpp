#pragma once

#include <map>

#include <bts/blockchain/types.hpp>

const static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS {
    {1, bts::blockchain::block_id_type("8abcfb93c52f999e3ef5288c4f837f4f15af5521")},
    {225000, bts::blockchain::block_id_type("2e09195c3e4ef6d58736151ea22f78f08556e6a9")},
    {434800, bts::blockchain::block_id_type("f3cd2d104efea2c36583d62a3887251194be5c43")}
};
