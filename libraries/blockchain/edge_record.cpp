#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/types.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {
    const obj_type edge_record::type = edge_object;

    edge_index_key edge_record::index_key()const
    {
        edge_index_key key;
        key.from = from;
        key.to = to;
        key.name = name;
        return key;
    }

    edge_index_key edge_record::reverse_index_key()const
    {
        edge_index_key key;
        key.from = to;
        key.to = from;
        key.name = name;
        return key;
    }

}} // bts::blockchain
