#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8523e28fde6eed4eb749a84e28c9c7b56870c9cc" ) },
    {  147000, bts::blockchain::block_id_type( "d8cb4d9c3c50709ef8a037b39c4262472d9addfa" ) }
};

} } // bts::blockchain
