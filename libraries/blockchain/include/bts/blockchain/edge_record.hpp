#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/io/raw_fwd.hpp>

namespace bts { namespace blockchain {

    struct edge_index_key;
    struct edge_record 
    {
        static const obj_type type;
        object_id_type     from;
        object_id_type     to;
        string             name;
        variant            value;

        edge_index_key   index_key()const;
        edge_index_key   reverse_index_key()const;
    };
    typedef fc::optional<edge_record> oedge_record;

    struct edge_index_key
    {
        edge_index_key( object_id_type f = 0, object_id_type t = 0, const string& n = "")
        :from(f),to(t),name(n){}

        object_id_type from;
        object_id_type to;
        string         name;

        friend bool operator == ( const edge_index_key& a, const edge_index_key& b )
        {
            return std::tie(a.from, a.to, a.name) == std::tie(b.from, b.to, b.name);
        }
        friend bool operator < ( const edge_index_key& a, const edge_index_key& b )
        {
            return std::tie(a.from, a.to, a.name) < std::tie(b.from, b.to, b.name);
        }
    };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::edge_index_key, (from)(to)(name) );
FC_REFLECT( bts::blockchain::edge_record, (from)(to)(name)(value) );
