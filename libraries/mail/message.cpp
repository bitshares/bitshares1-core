#include <bts/mail/message.hpp>
#include <fc/crypto/aes.hpp>

namespace bts { namespace mail {

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
