#pragma once

#include <bts/blockchain/condition.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/reflect/reflect.hpp>

#include <stdint.h>

namespace bts { namespace blockchain {

    // This is packed into the high 16 bits of the object ID
    enum obj_type
    {
        null_object = -1,
        base_object = 0,
        account_object = 1,
        asset_object = 2,
        edge_object = 3,
        user_auction_object = 4,
        throttled_auction_object = 5,
        site_object = 6
    };

    
    struct object_record
    {
        uint64_t                      short_id()const;
        obj_type                      type()const;

        object_record( obj_type type_arg = base_object, uint64_t id = 0)
        {
            this->set_id( type_arg, id );
            owner_object = _id;
        }
                    
        template<typename ObjectType>
        object_record(const ObjectType& o, obj_type type = base_object, uint64_t short_id = 0 )
        :_data( fc::raw::pack( o ) )
        {
            this->set_id( type, short_id );
            owner_object = _id;
        }
 
        template<typename ObjectType>
        void set_data( const ObjectType& o )
        {
           FC_ASSERT( ObjectType::type == this->type()  );
           //set_id( this->type(), this->short_id());
           _data = fc::raw::pack(o);
        }

        template<typename ObjectType>
        ObjectType as()const
        {
            FC_ASSERT( ObjectType::type == this->type(), "Casting to the wrong type! ${expected} != ${requested}", 
                     ("expected",type())("requested",ObjectType::type) );
            return fc::raw::unpack<ObjectType>(_data);
        }

        void                        set_id( object_id_type );
        void                        set_id( obj_type type, uint64_t number );
        void                        make_null();

        object_id_type              _id = 0; // Do not access directly, use short_id()
        variant                     user_data; // user-added metadata for all objects - actual application logic should go in derived class
        vector<char>                _data; // derived class properties

        // always use chain_interface->get_object_condition(obj)  instead of accessing this!
        // At least until we migrate all legacy object types
        multisig_condition          _owners;
        object_id_type              owner_object; // If this points to itself, then the condition is _owners

    };

    typedef optional<object_record> oobject_record;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::obj_type, (null_object)(base_object)(account_object)(asset_object)(edge_object)(user_auction_object)(throttled_auction_object)(site_object) );
FC_REFLECT( bts::blockchain::object_record, (_id)(user_data)(_owners)(_data)(owner_object) );
/*
namespace fc {
   void to_variant( const bts::blockchain::object_record& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::object_record& vo );
}
*/
