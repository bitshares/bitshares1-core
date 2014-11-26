#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {

    struct edge_record : object_record
    {
        object_id_type     from;
        object_id_type     to;
        string             name;
        variant            value;
    };


} } // bts::blockchain

FC_REFLECT( bts::blockchain::edge_record, (from)(to)(name)(value) );
