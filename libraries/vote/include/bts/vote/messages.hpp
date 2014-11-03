/**
  * This file contains the message types used in the ID Verifier public API. All clients seeking ID verification must
  * use the verfier_public_api call for all queries, and must pass ciphertext messages to it. This obscures the nature
  * and content of all ID Verification calls from an eavesdropper.
  *
  * See each message type below for documentation of the individual methods which may be called.
  */
#pragma once

#include <bts/mail/message.hpp>
#include <bts/vote/types.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/io/raw_variant.hpp>

namespace bts { namespace vote {
using namespace bts::mail;

const static message_type exception_message_type = message_type(user_defined + 1);
const static message_type verification_request_message_type = message_type(user_defined + 2);
const static message_type verification_response_message_type = message_type(user_defined + 3);
const static message_type get_verification_message_type = message_type(user_defined + 4);

/**
 * @brief An exception_message will be returned in the event that any error occurred while processing the call.
 *
 * Exception messages will be encrypted if possible, but if the sender's public key could not be derived, the exception
 * will be returned in the clear. In general, the sender's public key will be known if the call is well-formed, as all
 * calls must be signed.
 *
 * Clients should not send messages of this type; this message is to be returned by the server only.
 */
struct exception_message
{
   static const message_type type;

   fc::exception e;
};

/**
 * @brief An identity_verification_request_message indicates that the client wishes to initiate an ID verification.
 *
 * See documentation of identity_verifier::store_new_request() for further details.
 */
struct identity_verification_request_message
{
   static const message_type type;

   identity_verification_request request;
   fc::ecc::compact_signature signature;
};

/**
 * @brief An identity_verification_response_message will be returned by calls which query the verification result.
 *
 * Clients should not send messages of this type; this message is to be returned by the server only.
 */
struct identity_verification_response_message
{
   static const message_type type;

   ///Status and/or response to verification. A null response indicates that verification is still pending.
   optional<identity_verification_response> response;
};

/**
 * @brief A get_verification_message indicates that the client wishes to check the status or result of verification.
 *
 * See documentation of identity_verifier::get_verified_identity() for further details.
 */
struct get_verification_message
{
   static const message_type type;

   ///Public key of the identity being verified
   address owner;
   ///Signature on owner address above; required to verify that the caller is permitted to retrieve the result.
   fc::ecc::compact_signature owner_address_signature;
};

} } // namespace bts::vote

FC_REFLECT( bts::vote::exception_message, (e) )
FC_REFLECT( bts::vote::identity_verification_request_message, (request)(signature) )
FC_REFLECT( bts::vote::identity_verification_response_message, (response) )
FC_REFLECT( bts::vote::get_verification_message, (owner)(owner_address_signature) )
