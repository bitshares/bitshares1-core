#include <bts/blockchain/operations.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {
   const operation_type_enum withdraw_operation::type      = withdraw_op_type;
   const operation_type_enum deposit_operation::type       = deposit_op_type;
   const operation_type_enum create_asset_operation::type  = create_asset_op_type;
   const operation_type_enum update_asset_operation::type  = update_asset_op_type;
   const operation_type_enum issue_asset_operation::type   = issue_asset_op_type;
   const operation_type_enum reserve_name_operation::type = reserve_name_op_type;
   const operation_type_enum update_name_operation::type   = update_name_op_type;


   balance_id_type  deposit_operation::balance_id()const
   {
      return condition.get_address();
   }

   deposit_operation::deposit_operation( const address& owner, 
                                                     const asset& amnt, 
                                                     name_id_type delegate_id )
   {
      amount = amnt.amount;
      condition = withdraw_condition( withdraw_with_signature( owner ), amnt.asset_id, delegate_id );
   }

   reserve_name_operation::reserve_name_operation( const std::string& n, 
                                                   const std::string& d, 
                                                   const public_key_type& owner, 
                                                   const public_key_type& active, bool as_delegate )
   :name(n),json_data(d),owner_key(owner),active_key(active),is_delegate(as_delegate){}

} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::operation& var,  variant& vo )
   {
      using namespace bts::blockchain;
      mutable_variant_object obj( "type", var.type );
      switch( (operation_type_enum)var.type )
      {
         case withdraw_operation::type:
            obj["data"] =  fc::raw::unpack<withdraw_operation>( var.data );
            break;
         case deposit_operation::type:
            obj[ "data"] = fc::raw::unpack<deposit_operation>( var.data );
            break;
         case reserve_name_op_type:
            obj[ "data"] = fc::raw::unpack<reserve_name_operation>( var.data );
            break;
         case update_name_op_type:
            obj[ "data"] = fc::raw::unpack<update_name_operation>( var.data );
            break;
         case create_asset_op_type:
            obj[ "data"] = fc::raw::unpack<create_asset_operation>( var.data );
            break;
         case update_asset_op_type:
            obj[ "data"] = fc::raw::unpack<update_asset_operation>( var.data );
            break;
         case issue_asset_op_type:
            obj[ "data"] = fc::raw::unpack<issue_asset_operation>( var.data );
            break;
         case null_op_type:
            obj[ "data"] = nullptr;
            break;
      }
      vo = std::move(obj);
   }
   /** @todo implement */
   void from_variant( const variant& var,  bts::blockchain::operation& vo )
   {
      using namespace bts::blockchain;
      auto obj = var.get_object();
      from_variant( obj["type"], vo.type );
      switch( (operation_type_enum)vo.type )
      {
         case withdraw_operation::type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_operation>() );
            break;
         case deposit_operation::type:
            vo.data = fc::raw::pack( obj["data"].as<deposit_operation>() );
            break;
         case reserve_name_op_type:
            vo.data = fc::raw::pack( obj["data"].as<reserve_name_operation>() );
            break;
         case update_name_op_type:
            vo.data = fc::raw::pack( obj["data"].as<update_asset_operation>() );
            break;
         case create_asset_op_type:
            vo.data = fc::raw::pack( obj["data"].as<create_asset_operation>() );
            break;
         case update_asset_op_type:
            vo.data = fc::raw::pack( obj["data"].as<update_asset_operation>() );
            break;
         case issue_asset_op_type:
            vo.data = fc::raw::pack( obj["data"].as<issue_asset_operation>() );
            break;
         case null_op_type:
            break;
      }
   }
}
