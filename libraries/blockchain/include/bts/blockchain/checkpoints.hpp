#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8523e28fde6eed4eb749a84e28c9c7b56870c9cc" ) },
    {  138000, bts::blockchain::block_id_type( "a252492dba3e85ddd08eacfdcd3ad0124a022bc6" ) }
};

} } // bts::blockchain
