#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
   const uint8_t withdraw_with_signature::type    = withdraw_signature_type;
   const uint8_t withdraw_with_multi_sig::type    = withdraw_multi_sig_type;
   const uint8_t withdraw_with_password::type     = withdraw_password_type;
   const uint8_t withdraw_option::type            = withdraw_option_type;

   memo_status::memo_status( const memo_data& memo, 
                   bool valid_signature,
                   const fc::ecc::private_key& opk )
   :memo_data(memo),has_valid_signature(valid_signature),owner_private_key(opk){}

   void        memo_data::set_message( const std::string& message_str )
   {
      FC_ASSERT( message_str.size() <= sizeof( message ) );
      if( message_str.size() )
      {
         memcpy( message.data, message_str.c_str(), message_str.size() );
      }
   }

   std::string memo_data::get_message()const
   {
      return std::string( (const char*)&message, sizeof(message) ).c_str();
   }


   balance_id_type withdraw_condition::get_address()const
   {
      return address( *this );
   }
   omemo_status withdraw_with_signature::decrypt_memo_data( const fc::ecc::private_key& receiver_key )const
   { try {
      FC_ASSERT( memo.valid() );
//      ilog( "receiver_key: ${r}", ("r",receiver_key) );
      auto secret = receiver_key.get_shared_secret( memo->one_time_key );
//      ilog( "secret: ${secret}", ("secret",secret) );
      extended_private_key ext_receiver_key(receiver_key);
 //     ilog( "ext_receiver_key: ${key}",("key",ext_receiver_key) );
      
      fc::ecc::private_key secret_private_key = ext_receiver_key.child( fc::sha256::hash(secret), 
                                                                        extended_private_key::public_derivation );
   //   ilog( "secret_private_key: ${k}", ("k",secret_private_key)  );
      auto secret_public_key = secret_private_key.get_public_key();
    //  ilog( "secret_public_key: ${k}", ("k",secret_public_key)  );

      if( owner != address(secret_public_key) )
         return omemo_status();

     // ilog( "owner: ${o} == ${address}", ("o",owner)("address",address(secret_public_key)) );
      auto memo = decrypt_memo_data( secret );

      bool has_valid_signature = false;
      if( memo.memo_flags == from_memo )
      {
         auto check_secret = secret_private_key.get_shared_secret( memo.from );
         has_valid_signature = check_secret._hash[0] == memo.from_signature;
      }
      else
      {
         has_valid_signature = true;
      }

      return memo_status( memo, has_valid_signature, secret_private_key );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }


   void  withdraw_with_signature::encrypt_memo_data( const fc::ecc::private_key& one_time_private_key, 
                                   const fc::ecc::public_key&  to_public_key,
                                   const fc::ecc::private_key& from_private_key,
                                   const std::string& memo_message,
                                   const fc::ecc::public_key&  memo_pub_key,
                                   memo_flags_enum memo_type )
   {
      memo = titan_memo();
      auto secret = one_time_private_key.get_shared_secret( to_public_key );
//      ilog( "secret: ${s}", ("s",secret) );
      auto ext_to_public_key = extended_public_key(to_public_key);
//      ilog( "ext_to_public_key: ${k}", ("k",ext_to_public_key) );
      auto secret_ext_public_key = ext_to_public_key.child( fc::sha256::hash(secret) );
 //     ilog( "secret ext pub key: ${s}", ("s",secret_ext_public_key) );
      auto secret_public_key = secret_ext_public_key.get_pub_key();
 //     ilog( "secret_public_key: ${k}", ("k",secret_public_key) );
      owner = address( secret_public_key );
 //     ilog( "owner: ${owner}", ("owner",owner) );

      auto check_secret = from_private_key.get_shared_secret( secret_public_key );

      memo_data memo_content;
      memo_content.set_message( memo_message );
      memo_content.from = memo_pub_key;
      memo_content.from_signature = check_secret._hash[0];
      memo_content.memo_flags = memo_type;

      memo->one_time_key = one_time_private_key.get_public_key();

      encrypt_memo_data( secret, memo_content );
   }

   memo_data withdraw_with_signature::decrypt_memo_data( const fc::sha512& secret )const
   { try {
      FC_ASSERT( memo.valid() );
      return fc::raw::unpack<memo_data>( fc::aes_decrypt( secret, memo->encrypted_memo_data ) );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void withdraw_with_signature::encrypt_memo_data( const fc::sha512& secret, 
                                             const memo_data& memo_content )
   {
      FC_ASSERT( memo.valid() );
      memo->encrypted_memo_data = fc::aes_encrypt( secret, fc::raw::pack( memo_content ) );
   }

} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::withdraw_condition& var,  variant& vo )
   {
      using namespace bts::blockchain;
      fc::mutable_variant_object obj;
      obj["asset_id"] = var.asset_id;
      obj["delegate_id"] = var.delegate_id;
      obj["type"] =  var.type;

      switch( (withdraw_condition_types) var.type )
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
      from_variant( obj["type"], vo.type );

      switch( (withdraw_condition_types) vo.type )
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
   void to_variant( const bts::blockchain::memo_data& var,  variant& vo )
   {
      mutable_variant_object obj("from",var.from);
      obj("from_signature",var.from_signature)
         ("message",var.get_message())
         ("memo_flags",var.memo_flags);
      vo = std::move( obj );
   }
   void from_variant( const variant& var,  bts::blockchain::memo_data& vo )
   { try {
      const variant_object& obj = var.get_object();
      if( obj.contains( "from" ) )
         vo.from = obj["from"].as<bts::blockchain::public_key_type>();
      if( obj.contains( "from_signature" ) )
         vo.from_signature = obj["from_signature"].as_int64();
      if( obj.contains( "message" ) ) 
         vo.set_message( obj["message"].as_string() );
      if( obj.contains( "memo_flags") )
         vo.memo_flags = obj["memo_flags"].as<bts::blockchain::memo_flags_enum>(); 
   } FC_RETHROW_EXCEPTIONS( warn, "unable to convert variant to memo_data", ("variant",var) ) }

} // fc
