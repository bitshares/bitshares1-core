#pragma once
#include <bts/blockchain/types.hpp>
#include <map>

const static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {      1, bts::blockchain::block_id_type( "8abcfb93c52f999e3ef5288c4f837f4f15af5521" ) },
    { 225000, bts::blockchain::block_id_type( "2e09195c3e4ef6d58736151ea22f78f08556e6a9" ) },
    { 442700, bts::blockchain::block_id_type( "c22e2dc1954f7d3a9620048d64587018e98c03f8" ) },
    { 452700, bts::blockchain::block_id_type( "b678b7b86a8b05593df49acc5e8ba62f7177276e" ) }
};
