#include <bts/blockchain/withdraw_types.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {
   const uint8_t withdraw_with_signature::type    = withdraw_signature_type;
   const uint8_t withdraw_with_multi_sig::type    = withdraw_multi_sig_type;
   const uint8_t withdraw_with_password::type     = withdraw_password_type;
   const uint8_t withdraw_option::type            = withdraw_option_type;

   balance_id_type withdraw_condition::get_address()const
   {
      return address( *this );
   }
} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::withdraw_condition& var,  variant& vo )
   {
      using namespace bts::blockchain;
      fc::mutable_variant_object obj;
      obj["asset_id"] = var.asset_id;
      obj["delegate_id"] = var.delegate_id;
      obj["condition"] =  var.condition;

      switch( (withdraw_condition_types) var.condition )
      {
         case withdraw_signature_type:
            obj["data"] = fc::raw::unpack<withdraw_with_signature>( var.data );
            break;
         case withdraw_multi_sig_type:
            obj["data"] = fc::raw::unpack<withdraw_with_multi_sig>( var.data );
            break;
         case withdraw_password_type:
            obj["data"] = fc::raw::unpack<withdraw_with_password>( var.data );
            break;
         case withdraw_option_type:
            obj["data"] = fc::raw::unpack<withdraw_option>( var.data );
            break;
         case withdraw_null_type:
            obj["data"] = fc::variant();
            break;
      }
      vo = std::move( obj );
   }

   void from_variant( const variant& var,  bts::blockchain::withdraw_condition& vo )
   {
      using namespace bts::blockchain;
      auto obj = var.get_object();
      from_variant( obj["asset_id"], vo.asset_id );
      from_variant( obj["delegate_id"], vo.delegate_id );
      from_variant( obj["condition"], vo.condition );

      switch( (withdraw_condition_types) vo.condition )
      {
         case withdraw_signature_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_signature>() );
            break;
         case withdraw_multi_sig_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_multi_sig>() );
            break;
         case withdraw_password_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_password>() );
            break;
         case withdraw_option_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_option>() );
            break;
         case withdraw_null_type:
            break;
      }
   }
}
