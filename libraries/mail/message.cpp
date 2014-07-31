#include <bts/mail/message.hpp>
#include <fc/crypto/aes.hpp>

namespace bts { namespace mail {
   const message_type signed_email_message::type       = email;
   const message_type transaction_notice_message::type = transaction_notice;

   digest_type email_message::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   void                signed_email_message::sign( const fc::ecc::private_key& from_key )
   { try {
      from_signature = from_key.sign_compact( digest() );
   } FC_CAPTURE_AND_RETHROW() }

   fc::ecc::public_key signed_email_message::from()const
   { try {
      return fc::ecc::public_key( from_signature, digest() );
   } FC_CAPTURE_AND_RETHROW() }

   message_id_type message::id()const
   {
      fc::ripemd160::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   encrypted_message message::encrypt( const fc::ecc::private_key& onetimekey,
                                       const fc::ecc::public_key&  receiver_public_key )const
   {
      auto shared_secret = onetimekey.get_shared_secret( receiver_public_key );
      encrypted_message result;
      result.onetimekey = onetimekey.get_public_key();
      result.data = fc::aes_encrypt( shared_secret, fc::raw::pack( *this ) );
      return  result;
   }

   message encrypted_message::decrypt( const fc::ecc::private_key& e )const
   {
      auto shared_secret = e.get_shared_secret(onetimekey);
      auto decrypted_data = fc::aes_decrypt( shared_secret, data );
      return fc::raw::unpack<message>( decrypted_data );
   };

} } // bts::mail
