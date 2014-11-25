#pragma once
#include <stdint.h>
#include <bts/blockchain/types.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {

    enum obj_type
    {
        normal_object = 0,
        account_object = 1,
        asset_object = 2
    };

    struct obj_id
    {
        fc::enum_type<uint16_t,obj_type>        type;
        uint64_t                                number;

        obj_id(){}
        obj_id( uint64_t packed_int )
        {
            obj_id ret;
            type = type;
            number = packed_int & 0x0000ffffffffffff;
        }

        uint64_t as_int()
        {
            return (uint64_t(type.value) << 48) | number;
        }
        operator int64_t() { return as_int(); }

        friend bool operator == ( const obj_id& a, const obj_id& b )
        {
            return a.type == b.type && a.number == b.number;
        }
        friend bool operator < ( const obj_id& a, const obj_id& b )
        {
            return std::tie(a.type, a.number) < std::tie(b.type, b.number);
        }
    };

    typedef uint64_t object_id_type;

    struct object_record
    {
        object_id_type              id;
        variant                     user_data; // user-added metadata for all objects - actual application logic should go in derived class

        // always use chain_interface->get_object_owners(obj)  instead of accessing this!
        // At least until we migrate all legacy object types
        set<address>                _owners;

        object_record() {}
        object_record( const object_id_type& id ):id(id){}
    };

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::obj_type, (normal_object)(account_object)(asset_object) );
FC_REFLECT( bts::blockchain::object_record, (id)(user_data)(_owners) );
FC_REFLECT( bts::blockchain::obj_id, (type)(number) );
