#pragma once
#include <stdint.h>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/condition.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/reflect/reflect.hpp>

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

        virtual ~object_record() {}
        object_record( const object_id_type& id ):_id(id){}
        object_record( const obj_type& type, const uint64_t& id )
        {
            this->set_id( type, id );
            owner_object = 0;
        }

        object_record()
        {
            this->set_id( base_object, 0 );
            owner_object = 0;
        }

        object_record( const object_record& o )
            :_id(o._id),user_data(o.user_data),_data(o._data),_owners(o._owners),owner_object(o.owner_object)
        {
        }

        object_record( object_record&& o )
            :_data(std::move( o._data ))
        {
             _id = o._id;
            _owners = o._owners;
            user_data = o.user_data;
            owner_object = o.owner_object;
        }

        object_record& operator=( const object_record& o )
        {
            if( this == &o ) return *this;
            _id = o._id;
            _data = o._data;
            _owners = o._owners;
            user_data = o.user_data;
            owner_object = o.owner_object;
            return *this;
        }

        object_record& operator=( object_record&& o )
        {
            if( this == &o ) return *this;
             _id = o._id;
             _data = std::move( o._data );
             _owners = o._owners;
             user_data = o.user_data;
             owner_object = o.owner_object;
             return *this;
        }    
             
             
        template<typename ObjectType>
        object_record(const ObjectType& o)
            :_data( fc::raw::pack( o ) )
        {
            set_id( ObjectType::type, o.short_id() );
            user_data = o.user_data;
            _owners = o._owners;
            owner_object = o.owner_object;
            _data = fc::raw::pack<ObjectType>( o );
        }

        template<typename ObjectType>
        ObjectType as()const
        {
            FC_ASSERT( ObjectType::type == this->type(), "Casting to the wrong type!" );
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
