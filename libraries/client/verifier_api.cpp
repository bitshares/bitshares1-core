#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/mail/message.hpp>
#include <bts/vote/messages.hpp>

#define SANITY_CHECK FC_ASSERT( _id_verifier, "ID Verifier must be enabled to use its API." );

namespace bts { namespace client { namespace detail {
using namespace bts::vote;

void sign_identity(identity& id, const private_key_type& key)
{
   fc::time_point_sec valid_from = fc::time_point::now();
   fc::time_point_sec valid_until = valid_from + fc::days(365);

   for( signed_identity_property property : id.properties )
      property.verifier_signatures.push_back(signature_data::sign(key, property.id(id.owner), valid_from, valid_until));
}

void sign_identity_if_necessary(identity_verification_response_message& response, const private_key_type& my_key)
{
   //Did we accept the verification request? If so, sign the identity.
   if( response.response && response.response->accepted && response.response->verified_identity )
      //TODO: Cache these signatures
      sign_identity(*response.response->verified_identity, my_key);
}

mail::message client_impl::verifier_store_new_request(const mail::message& const_request)
{
   SANITY_CHECK;
   fc::optional<public_key_type> sender_public_key;
   message request_message = const_request;
   message reply;

   try {
      FC_ASSERT( request_message.type == encrypted, "Do not send plaintexts to the identity verifier!" );

      //FIXME: How should I know what account to verify with? I'm currently assuming it's the one the client addressed
      //the request to, but that's kind of iffy. Do I trust the client that much?...
      owallet_account_record my_account = _wallet->get_account_for_address(request_message.recipient);
      FC_ASSERT( my_account && my_account->is_my_account, "Message is addressed to someone else!" );
      auto my_key = _wallet->get_private_key(my_account->active_key());
      request_message = request_message.as<encrypted_message>().decrypt(my_key);

      FC_ASSERT( request_message.type == verification_request_message_type, "Unexpected message type ${t}",
                 ("t", request_message.type) );

      identity_verification_request_message request = request_message.as<identity_verification_request_message>();
      sender_public_key = fc::ecc::public_key(request.signature, request.request.digest());

      auto response = identity_verification_response_message({_id_verifier->store_new_request(request.request,
                                                              request.signature)});
      sign_identity_if_necessary(response, my_key);

      reply = response;
   } catch (const fc::exception& e) {
      reply = exception_message({e});
   }

   if( sender_public_key )
   {
      reply = reply.encrypt(fc::ecc::private_key::generate(), *sender_public_key);
      reply.recipient = *sender_public_key;
   }

   return reply;
}

} } } // namespace bts::client::detail
