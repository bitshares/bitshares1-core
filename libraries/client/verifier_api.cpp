#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/mail/message.hpp>
#include <bts/vote/messages.hpp>

#include <fc/crypto/digest.hpp>

#define SANITY_CHECK FC_ASSERT( _id_verifier, "ID Verifier must be enabled to use its API." );

namespace bts { namespace client { namespace detail {
using namespace bts::vote;

void sign_identity(identity& id, const private_key_type& key, fc::time_point_sec expiration)
{
   fc::time_point_sec valid_from = fc::time_point::now();
   id.sign(key, valid_from, expiration);
}

void sign_identity_if_necessary(identity_verification_response_message& response, const private_key_type& my_key)
{
   //Did we accept the verification request? If so, sign the identity.
   if( response.response && response.response->accepted && response.response->verified_identity )
      //TODO: Cache these signatures
      sign_identity(*response.response->verified_identity, my_key, *response.response->expiration_date);
}

mail::message client_impl::verifier_public_api(const message& const_request)
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

      switch(short(request_message.type))
      {
      case verification_request_message_type: {
         //Parse message, check signature
         identity_verification_request_message call = request_message.as<identity_verification_request_message>();
         sender_public_key = fc::ecc::public_key(call.signature, call.request.digest());
         FC_ASSERT( *sender_public_key == call.request.owner, "Caller is not owner of identity." );

         //Make actual call, sign response
         identity_verification_response_message response({_id_verifier->store_new_request(call.request,
                                                                                          *sender_public_key)});
         sign_identity_if_necessary(response, my_key);
         reply = response;
         break;
      } case get_verification_message_type: {
         //Parse message, check signature
         get_verification_message call = request_message.as<get_verification_message>();
         sender_public_key = fc::ecc::public_key(call.owner_address_signature, fc::digest(call.owner));
         FC_ASSERT( *sender_public_key == call.owner, "Caller is not owner of identity." );

         //Make actual call, sign response
         identity_verification_response_message response({_id_verifier->get_verified_identity(call.owner)});
         sign_identity_if_necessary(response, my_key);
         reply = response;
         break;
      } default:
         FC_THROW( "Unknown message type. Cannot evaluate call." );
      }
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

vector<identity_verification_request_summary> client_impl::verifier_list_requests(
      const vote::request_status_enum& status,
      const fc::microseconds &after_time,
      uint32_t limit) const
{
   SANITY_CHECK;
   return _id_verifier->list_requests(status, after_time, limit);
}

identity_verification_request client_impl::verifier_peek_request(const fc::microseconds& request_id) const
{
   SANITY_CHECK;
   return _id_verifier->peek_request(request_id);
}

identity_verification_request client_impl::verifier_take_pending_request(const fc::microseconds &request_id)
{
   SANITY_CHECK;
   return _id_verifier->take_pending_request(request_id);
}

optional<identity_verification_request> client_impl::verifier_take_next_request()
{
   SANITY_CHECK;
   return _id_verifier->take_next_request();
}

void client_impl::verifier_resolve_request(const fc::microseconds &request_id,
                                           const identity_verification_response &response,
                                           bool skip_soft_checks)
{
   SANITY_CHECK;
   _id_verifier->resolve_request(request_id, response, skip_soft_checks);
}

fc::variant client_impl::verifier_review_record(const fc::microseconds &request_id) const
{
   SANITY_CHECK;
   return _id_verifier->fetch_record(request_id);
}
} } } // namespace bts::client::detail
