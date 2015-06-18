#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8523e28fde6eed4eb749a84e28c9c7b56870c9cc" ) },
    {   25001, bts::blockchain::block_id_type( "f5bbb4c5728d809e3d0877354964ea24e9961904" ) },
    {  120001, bts::blockchain::block_id_type( "1557fee97884c893aaebba9a1fed803e7fd005d9" ) },
    {  129601, bts::blockchain::block_id_type( "18bcec3430d35538470d7fddc2a51a28cb71fcde" ) },
    {  172718, bts::blockchain::block_id_type( "e452dd44902bc86110923285da03ac12e8cfbe6f" ) },
    {  173585, bts::blockchain::block_id_type( "1d1695bc8ebd3ebae0168007afb2912269adbef1" ) },
    {  187001, bts::blockchain::block_id_type( "72d06a6ba3e79c02327d8048852f9e51b6c8ea71" ) },
    {  237501, bts::blockchain::block_id_type( "42d7bb317f51082f7faffee3f7c79bf59f47acac" ) },
    {  650001, bts::blockchain::block_id_type( "f5d74cc87e68adf44af8c7b5528d3716e70f7102" ) },
    { 1275700, bts::blockchain::block_id_type( "d9dec580e5797b607065615c289dd866f5c647f6" ) }
};

// Initialized in load_checkpoints()
static uint32_t LAST_CHECKPOINT_BLOCK_NUM = 0;

} } // bts::blockchain
