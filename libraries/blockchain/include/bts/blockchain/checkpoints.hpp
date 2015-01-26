#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8523e28fde6eed4eb749a84e28c9c7b56870c9cc" ) },
    {  171000, bts::blockchain::block_id_type( "f926a1ef1610c35bd2cd8152aad21265ccd9fbc3" ) }
};

} } // bts::blockchain
