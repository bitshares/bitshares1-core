#pragma once
#include <stdint.h>
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

    enum obj_type
    {
        normal_object = 0,
        account_object = 1,
        asset_object = 2
    };

    struct bitfield48
    {
        uint64_t value : 48;
        friend bool operator < ( const bitfield48& a, const bitfield48& b )
        {
            return a.value < b.value;
        }
        friend bool operator == ( const bitfield48& a, const bitfield48& b )
        {
            return a.value == b.value;
        }
    };

    struct obj_id
    {
        obj_type       type;
        bitfield48     number;

        friend bool operator == ( const obj_id& a, const obj_id& b )
        {
            return a.type == b.type && a.number == b.number;
        }
        friend bool operator < ( const obj_id& a, const obj_id& b )
        {
            return std::tie(a.type, a.number) < std::tie(b.type, b.number);
        }

    };
    typedef struct obj_id object_id_type;

    struct object_record
    {
        object_id_type              id;
        variant                     public_data;
        optional<set<address>>      owners;
        set<address>                get_owners(); // if owners is null, will try special owner logic
    };

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::obj_type, (normal_object)(account_object)(asset_object) );
FC_REFLECT( bts::blockchain::object_record, (id)(public_data)(owners) );
