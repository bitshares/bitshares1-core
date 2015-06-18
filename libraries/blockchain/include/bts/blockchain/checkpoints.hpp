#pragma once
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8abcfb93c52f999e3ef5288c4f837f4f15af5521" ) },
    {  274001, bts::blockchain::block_id_type( "f46c109cfb1bac323122ae59b08edc23328d880c" ) },
    {  316002, bts::blockchain::block_id_type( "206b7e6574019f4352515bd3d96162fb40a1b18a" ) },
    {  340001, bts::blockchain::block_id_type( "d3858436100abe3ad7ee9f026e5f7f4781732e06" ) },
    {  357001, bts::blockchain::block_id_type( "d240848ca9bf4be30204eded02fb35ce2e215d41" ) },
    {  408751, bts::blockchain::block_id_type( "12481e818abe9bc86069e45586df11d2cff7dbb8" ) },
    {  451501, bts::blockchain::block_id_type( "42e519db48f09570fcc02f38288648a92789dcb3" ) },
    {  494001, bts::blockchain::block_id_type( "59836f51f13e07be1de734a02360742c7b5f0dd6" ) },
    {  554801, bts::blockchain::block_id_type( "4a992cc282a0024d393c6389cf84cbb8df1fc839" ) },
    {  578901, bts::blockchain::block_id_type( "add6131c7bc6f1c9470de209b0e9c257d664f1c9" ) },
    {  613201, bts::blockchain::block_id_type( "3317ea0a272976a10ef6593e840f9ff2032affad" ) },
    {  640001, bts::blockchain::block_id_type( "11c748cdbfc445396deb6d24908521701aaa80aa" ) },
    {  820201, bts::blockchain::block_id_type( "904278b4bd585e3055c092aada1e030fbdf69a0e" ) },
    {  871001, bts::blockchain::block_id_type( "137ee1e9171a7ed1fe9cc58536d8e20749c3f199" ) },
    {  991701, bts::blockchain::block_id_type( "2e48f0a16d2107b9ab3d1c87e803ee70d3c91ce0" ) },
    { 1315315, bts::blockchain::block_id_type( "baede744122d4b8d296ab3497bf5c849d1131466" ) },
    { 1575501, bts::blockchain::block_id_type( "52c5e5764fba4c876cde8c80e598c89ee5c35d9f" ) },
    { 1772201, bts::blockchain::block_id_type( "5832516564d86e86edbbb501cabec98c77504307" ) },
    { 2071001, bts::blockchain::block_id_type( "58847f93f0d1846cb9250d7ba342bebda6e698e4" ) },
    { 2113001, bts::blockchain::block_id_type( "3327c3632f71e470c6f875c7b1798abf9090535d" ) },
    { 2839000, bts::blockchain::block_id_type( "c1d25d2680f34748102203570e391737afa29b46" ) }
};

// Initialized in load_checkpoints()
static uint32_t LAST_CHECKPOINT_BLOCK_NUM = 0;

} } // bts::blockchain
