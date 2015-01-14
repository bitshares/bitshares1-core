#pragma once
#include <bts/blockchain/types.hpp>
#include <map>

static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS
{
    {       1, bts::blockchain::block_id_type( "8abcfb93c52f999e3ef5288c4f837f4f15af5521" ) },
    {  100000, bts::blockchain::block_id_type( "96f98d49722848a6a47ad04aece8b9f93c9e9c23" ) },
    {  200000, bts::blockchain::block_id_type( "222ddc49db592103c51ad22cdc4140185ef564d9" ) },
    {  300000, bts::blockchain::block_id_type( "0d1d0b8f7f1f4590f8e083edc03869383ed74e3e" ) },
    {  400000, bts::blockchain::block_id_type( "053d398b6597d5c61365afd100d87b824bf49f65" ) },
    {  500000, bts::blockchain::block_id_type( "f02910a7115fb826984ce3a432cb371d5d7a99b8" ) },
    {  600000, bts::blockchain::block_id_type( "f278db8722235343c9db9f077fe67c54a5f25f3b" ) },
    {  700000, bts::blockchain::block_id_type( "f120ff9b159661b9ac084a0a69c58dcbd79cbb49" ) },
    {  800000, bts::blockchain::block_id_type( "0e94ad17c598e42e5ec2f522a05bf4df6a1778da" ) },
    {  900000, bts::blockchain::block_id_type( "a78e09bf1fd13bcd7b0959bc0764fe45db0a652d" ) },
    { 1000000, bts::blockchain::block_id_type( "20da1352f86f0fc7d5b42f5e96be86ea5b219b6f" ) },
    { 1100000, bts::blockchain::block_id_type( "bec7da4758ad453a49a9d9d4128acb92a1fdfd04" ) },
    { 1200000, bts::blockchain::block_id_type( "9e514e0f0a17d78be7c0045f9f22a253812c6c92" ) },
    { 1300000, bts::blockchain::block_id_type( "623e5c04a77b7e688299a45bfb1d84a0bf3ea318" ) },
    { 1400000, bts::blockchain::block_id_type( "f7645d369b7a818c4acb94f3bedbcb7747dea9dd" ) },
    { 1500000, bts::blockchain::block_id_type( "a9027b880105fa89f4d0c2bf9a0df328fed3108d" ) },
    { 1523000, bts::blockchain::block_id_type( "bc98cb0ad0718b5225d8dad95dda565b97575818" ) }
};
