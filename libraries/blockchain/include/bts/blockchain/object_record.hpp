#pragma once
#include <stdint.h>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/condition.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {

    // This is packed into the high 16 bits of the object ID
    enum obj_type
    {
        null_object = -1,
        base_object = 0,
        account_object = 1,
        asset_object = 2,
        edge_object = 3
    };

    
    struct object_record
    {
        uint64_t                      short_id()const;
        obj_type                      type()const;

        object_record() {}
        virtual ~object_record() {}
        object_record( const object_id_type& id ):_id(id){}
        object_record( const obj_type& type, const uint64_t& id )
        {
            this->set_id( type, id );
        }

        template<typename ObjectType>
        object_record(const ObjectType& o)
        {
            _data = fc::raw::pack( o );
        }

        template<typename ObjectType>
        ObjectType as()const
        {
            FC_ASSERT( ObjectType::type == this->type(), "Casting to the wrong type!" );
            return fc::raw::unpack<ObjectType>(_data);
        }

        void                        set_id( obj_type type, uint64_t number );
        void                        make_null(); // default object has "base object" type, not null type


        object_id_type              _id = 0; // Do not access directly, use short_id()
        variant                     user_data; // user-added metadata for all objects - actual application logic should go in derived class
        std::vector<char>           _data; // derived class properties

        // always use chain_interface->get_object_owners(obj)  instead of accessing this!
        // At least until we migrate all legacy object types
        multisig_condition          _owners;

    };

    typedef optional<object_record> oobject_record;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::obj_type, (null_object)(base_object)(account_object)(asset_object)(edge_object) );
FC_REFLECT( bts::blockchain::object_record, (_id)(user_data)(_owners)(_data) );
