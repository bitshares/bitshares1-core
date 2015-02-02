#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::unordered_map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8523e28fde6eed4eb749a84e28c9c7b56870c9cc" ) },
    {  172718, bts::blockchain::block_id_type( "e452dd44902bc86110923285da03ac12e8cfbe6f" ) },
    {  173585, bts::blockchain::block_id_type( "1d1695bc8ebd3ebae0168007afb2912269adbef1" ) },
    {  192000, bts::blockchain::block_id_type( "80910335f384f420930351deac69debd007f74c0" ) }
};

// Initialized in load_checkpoints()
static uint32_t LAST_CHECKPOINT_BLOCK_NUM = 0;

} } // bts::blockchain
