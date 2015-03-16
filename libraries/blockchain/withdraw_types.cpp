#include <bts/blockchain/extended_address.hpp>
#include <bts/blockchain/withdraw_types.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/algorithm/string.hpp>

namespace bts { namespace blockchain {

   const uint8_t withdraw_with_signature::type    = withdraw_signature_type;
   const uint8_t withdraw_vesting::type           = withdraw_vesting_type;
   const uint8_t withdraw_with_multisig::type     = withdraw_multisig_type;
   const uint8_t withdraw_with_escrow::type       = withdraw_escrow_type;

   memo_status::memo_status( const extended_memo_data& memo, bool valid_signature,
                             const fc::ecc::private_key& opk )
   :extended_memo_data(memo),has_valid_signature(valid_signature),owner_private_key(opk)
   {
   }

   void memo_data::set_message( const std::string& message_str )
   {
      if( message_str.empty() ) return;
      FC_ASSERT( message_str.size() <= sizeof( message ) );
      memcpy( message.data, message_str.c_str(), message_str.size() );
   }

   void extended_memo_data::set_message( const std::string& message_str )
   {
      if( message_str.empty() ) return;
      FC_ASSERT( message_str.size() <= sizeof( message ) + sizeof( extra_message ) );
      if( message_str.size() <= sizeof( message ) )
      {
         memcpy( message.data, message_str.c_str(), message_str.size() );
      }
      else
      {
         memcpy( message.data, message_str.c_str(), sizeof( message ) );
         memcpy( extra_message.data, message_str.c_str() + sizeof( message ), message_str.size() - sizeof( message ) );
      }
   }

   std::string memo_data::get_message()const
   {
      // add .c_str() to auto-truncate at null byte
      return std::string( (const char*)&message, sizeof(message) ).c_str();
   }

   std::string extended_memo_data::get_message()const
   {
      // add .c_str() to auto-truncate at null byte
      return (std::string( (const char*)&message, sizeof(message) )
             + std::string( (const char*)&extra_message, sizeof(extra_message) )).c_str();
   }

   balance_id_type withdraw_condition::get_address()const
   {
      return address( *this );
   }

   set<address> withdraw_condition::owners()const
   {
       switch( withdraw_condition_types( type ) )
       {
           case withdraw_signature_type:
               return set<address>{ this->as<withdraw_with_signature>().owner };
           case withdraw_vesting_type:
               return set<address>{ this->as<withdraw_vesting>().owner };
           case withdraw_multisig_type:
               return this->as<withdraw_with_multisig>().owners;
           case withdraw_escrow_type:
           {
               const auto escrow = this->as<withdraw_with_escrow>();
               return set<address>{ escrow.sender, escrow.receiver, escrow.escrow };
           }
           default:
               return set<address>();
       }
   }

   optional<address> withdraw_condition::owner()const
   {
       switch( withdraw_condition_types( type ) )
       {
           case withdraw_signature_type:
               return this->as<withdraw_with_signature>().owner;
           case withdraw_vesting_type:
               return this->as<withdraw_vesting>().owner;
           default:
               return optional<address>();
       }
   }

   string withdraw_condition::type_label()const
   {
      string label = string( this->type );
      label = label.substr( 9 );
      label = label.substr( 0, label.find( "_" ) );
      boost::to_upper( label );
      return label;
   }

   omemo_status withdraw_with_signature::decrypt_memo_data( const fc::ecc::private_key& receiver_key, bool ignore_owner )const
   { try {
      try {
         FC_ASSERT( memo.valid() );
         auto secret = receiver_key.get_shared_secret( memo->one_time_key );
         extended_private_key ext_receiver_key(receiver_key);

         fc::ecc::private_key secret_private_key = ext_receiver_key.child( fc::sha256::hash(secret),
                                                                           extended_private_key::public_derivation );
         auto secret_public_key = secret_private_key.get_public_key();

         if( !ignore_owner && owner != address( secret_public_key ) )
            return omemo_status();

         auto memo = decrypt_memo_data( secret );

         bool has_valid_signature = false;
         if( memo.memo_flags == from_memo && !( memo.from == public_key_type() && memo.from_signature == 0 ) )
         {
            auto check_secret = secret_private_key.get_shared_secret( memo.from );
            has_valid_signature = check_secret._hash[0] == memo.from_signature;
         }
         else
         {
            has_valid_signature = true;
         }

         return memo_status( memo, has_valid_signature, secret_private_key );
      }
      catch( const fc::aes_exception& e )
      {
         return omemo_status();
      }
   } FC_CAPTURE_AND_RETHROW( (ignore_owner) ) }

   public_key_type withdraw_with_signature::encrypt_memo_data(const fc::ecc::private_key& one_time_private_key,
                                                              const fc::ecc::public_key& to_public_key,
                                                              const fc::ecc::private_key& from_private_key,
                                                              const std::string& memo_message,
                                                              bool use_stealth_address)
   {
      memo = titan_memo();
      const auto secret = one_time_private_key.get_shared_secret( to_public_key );
      const auto ext_to_public_key = extended_public_key( to_public_key );
      const auto secret_ext_public_key = ext_to_public_key.child( fc::sha256::hash( secret ) );
      const auto secret_public_key = use_stealth_address?
               secret_ext_public_key.get_pub_key() : to_public_key;
      owner = address( secret_public_key );

      fc::sha512 check_secret;
      if( from_private_key.get_secret() != fc::ecc::private_key().get_secret() )
        check_secret = from_private_key.get_shared_secret( secret_public_key );

      if( memo_message.size() <= BTS_BLOCKCHAIN_MAX_MEMO_SIZE )
      {
         memo_data memo_content;
         memo_content.set_message( memo_message );
         memo_content.from = from_private_key.get_public_key();
         memo_content.from_signature = check_secret._hash[0];
         memo->one_time_key = one_time_private_key.get_public_key();
         encrypt_memo_data( secret, memo_content );
      }
      else
      {
         extended_memo_data memo_content;
         memo_content.set_message( memo_message );
         memo_content.from = from_private_key.get_public_key();
         memo_content.from_signature = check_secret._hash[0];
         memo->one_time_key = one_time_private_key.get_public_key();
         encrypt_memo_data( secret, memo_content );
      }
      return secret_public_key;
   }

   extended_memo_data withdraw_with_signature::decrypt_memo_data( const fc::sha512& secret )const
   { try {
      FC_ASSERT( memo.valid() );
      if( memo->encrypted_memo_data.size() > ( sizeof( memo_data ) ) )
      {
         return fc::raw::unpack<extended_memo_data>( fc::aes_decrypt( secret, memo->encrypted_memo_data ) );
      }
      else
      {
         auto dmemo = fc::raw::unpack<memo_data>( fc::aes_decrypt( secret, memo->encrypted_memo_data ) );
         extended_memo_data result;
         result.from = dmemo.from;
         result.from_signature = dmemo.from_signature;
         result.message = dmemo.message;
         result.memo_flags = dmemo.memo_flags;
         return result;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void withdraw_with_signature::encrypt_memo_data( const fc::sha512& secret,
                                                    const memo_data& memo_content )
   {
      FC_ASSERT( memo.valid() );
      memo->encrypted_memo_data = fc::aes_encrypt( secret, fc::raw::pack( memo_content ) );
   }
   void withdraw_with_signature::encrypt_memo_data( const fc::sha512& secret,
                                                    const extended_memo_data& memo_content )
   {
      FC_ASSERT( memo.valid() );
      memo->encrypted_memo_data = fc::aes_encrypt( secret, fc::raw::pack( memo_content ) );
   }

   omemo_status withdraw_with_escrow::decrypt_memo_data( const fc::ecc::private_key& receiver_key, bool ignore_owner )const
   { try {
       try {
         FC_ASSERT( memo.valid() );
         auto secret = receiver_key.get_shared_secret( memo->one_time_key );
         extended_private_key ext_receiver_key(receiver_key);

         fc::ecc::private_key secret_private_key = ext_receiver_key.child( fc::sha256::hash(secret),
                                                                           extended_private_key::public_derivation );
         auto secret_public_key = secret_private_key.get_public_key();

         if( !ignore_owner
             && sender != address( secret_public_key )
             && receiver != address( secret_public_key )
             && escrow != address( secret_public_key )  )
            return omemo_status();

         auto memo = decrypt_memo_data( secret );

         bool has_valid_signature = false;
         if( memo.memo_flags == from_memo && !( memo.from == public_key_type() && memo.from_signature == 0 ) )
         {
            auto check_secret = secret_private_key.get_shared_secret( memo.from );
            has_valid_signature = check_secret._hash[0] == memo.from_signature;
         }
         else
         {
            has_valid_signature = true;
         }

         return memo_status( memo, has_valid_signature, secret_private_key );
      }
      catch ( const fc::aes_exception& e )
      {
         return omemo_status();
      }
   } FC_CAPTURE_AND_RETHROW( (ignore_owner) ) }

   public_key_type withdraw_with_escrow::encrypt_memo_data(
           const fc::ecc::private_key& one_time_private_key,
           const fc::ecc::public_key&  to_public_key,
           const fc::ecc::private_key& from_private_key,
           const std::string& memo_message )
   {
      memo = titan_memo();
      const auto secret = one_time_private_key.get_shared_secret( to_public_key );
      const auto ext_to_public_key = extended_public_key( to_public_key );
      const auto secret_ext_public_key = ext_to_public_key.child( fc::sha256::hash( secret ) );
      const auto secret_public_key = secret_ext_public_key.get_pub_key();

      sender   = address( one_time_private_key.get_public_key() );
      receiver = address( secret_public_key );

      fc::sha512 check_secret;
      if( from_private_key.get_secret() != fc::ecc::private_key().get_secret() )
        check_secret = from_private_key.get_shared_secret( secret_public_key );

      memo_data memo_content;
      memo_content.set_message( memo_message );
      memo_content.from = from_private_key.get_public_key();
      memo_content.from_signature = check_secret._hash[0];

      memo->one_time_key = one_time_private_key.get_public_key();

      encrypt_memo_data( secret, memo_content );
      return secret_public_key;
   }

   extended_memo_data withdraw_with_escrow::decrypt_memo_data( const fc::sha512& secret )const
   { try {
      FC_ASSERT( memo.valid() );
      if( memo->encrypted_memo_data.size() > ( sizeof( memo_data ) ) )
      {
         return fc::raw::unpack<extended_memo_data>( fc::aes_decrypt( secret, memo->encrypted_memo_data ) );
      }
      else
      {
         auto dmemo = fc::raw::unpack<memo_data>( fc::aes_decrypt( secret, memo->encrypted_memo_data ) );
         extended_memo_data result;
         result.from = dmemo.from;
         result.from_signature = dmemo.from_signature;
         result.message = dmemo.message;
         result.memo_flags = dmemo.memo_flags;
         return result;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void withdraw_with_escrow::encrypt_memo_data( const fc::sha512& secret,
                                                 const extended_memo_data& memo_content )
   {
      FC_ASSERT( memo.valid() );
      memo->encrypted_memo_data = fc::aes_encrypt( secret, fc::raw::pack( memo_content ) );
   }

   void withdraw_with_escrow::encrypt_memo_data( const fc::sha512& secret,
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
      obj["slate_id"] = var.slate_id;
      obj["type"] =  var.type;

      switch( (withdraw_condition_types) var.type )
      {
         case withdraw_null_type:
            obj["data"] = fc::variant();
            break;
         case withdraw_signature_type:
            obj["data"] = fc::raw::unpack<withdraw_with_signature>( var.data );
            break;
         case withdraw_vesting_type:
            obj["data"] = fc::raw::unpack<withdraw_vesting>( var.data );
            break;
         case withdraw_multisig_type:
            obj["data"] = fc::raw::unpack<withdraw_with_multisig>( var.data );
            break;
         case withdraw_escrow_type:
            obj["data"] = fc::raw::unpack<withdraw_with_escrow>( var.data );
            break;
         // No default to force compiler warning
      }
      vo = std::move( obj );
   }

   void from_variant( const variant& var,  bts::blockchain::withdraw_condition& vo )
   {
      using namespace bts::blockchain;
      auto obj = var.get_object();
      from_variant( obj["asset_id"], vo.asset_id );
      try
      {
          from_variant( obj["slate_id"], vo.slate_id );
      }
      catch( const fc::key_not_found_exception& )
      {
          from_variant( obj["delegate_slate_id"], vo.slate_id );
      }
      from_variant( obj["type"], vo.type );

      switch( (withdraw_condition_types) vo.type )
      {
         case withdraw_null_type:
            return;
         case withdraw_signature_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_signature>() );
            return;
         case withdraw_vesting_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_vesting>() );
            return;
         case withdraw_multisig_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_multisig>() );
            return;
         case withdraw_escrow_type:
            vo.data = fc::raw::pack( obj["data"].as<withdraw_with_escrow>() );
            return;
         // No default to force compiler warning
      }
      FC_ASSERT( false, "Invalid withdraw condition!" );
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
