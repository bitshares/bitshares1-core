#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/types.hpp>
#include <fc/exception/exception.hpp>


namespace bts { namespace blockchain {

    obj_type object_record::type()const
    {
        return obj_type(_id >> 48);
    }

    uint64_t object_record::short_id()const
    {
        return obj_type(_id & 0x0000ffffffffffff);
    }

    void     object_record::set_id( obj_type type, uint64_t number )
    {
        _id = (uint64_t(type) << 48) | number;
    }

    void     object_record::make_null()
    {
        _id = -1;
    }

    void     object_record::set_id( object_id_type id)
    {
        _id =  id;
    }

}} // bts::blockchain
/*
namespace fc {
   void to_variant( const bts::blockchain::object_record& var,  variant& vo )
   {
   }

   void from_variant( const variant& var,  bts::blockchain::object_record& vo )
   {
   }
}
*/
