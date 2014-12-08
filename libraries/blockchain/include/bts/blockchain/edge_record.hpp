#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {

    struct edge_index_key;
    struct edge_record : object_record
    {
        static const obj_type type = edge_object;
        object_id_type     from;
        object_id_type     to;
        string             name;
        variant            value;

        edge_record() { this->set_id( edge_object, 0 ); }
        edge_index_key   index_key();
        edge_index_key   reverse_index_key();
    };
    typedef fc::optional<edge_record> oedge_record;

    struct edge_index_key
    {
        object_id_type from = 0;
        object_id_type to = 0;
        string name = "";

        friend bool operator == ( const edge_index_key& a, const edge_index_key& b )
        {
            return a.from == b.from && a.to == b.to && a.name == b.name;
        }
        friend bool operator < ( const edge_index_key& a, const edge_index_key& b )
        {
            return std::tie(a.from, a.to, a.name) < std::tie(b.from, b.to, b.name);
        }
    };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::edge_record, (from)(to)(name)(value) );
FC_REFLECT( bts::blockchain::edge_index_key, (from)(to)(name) );
