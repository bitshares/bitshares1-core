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

struct exception_message
{
   static const message_type type;

   fc::exception e;
};

struct identity_verification_request_message
{
   static const message_type type;

   identity_verification_request request;
   fc::ecc::compact_signature signature;
};

struct identity_verification_response_message
{
   static const message_type type;

   optional<identity_verification_response> response;
};
} } // namespace bts::vote

FC_REFLECT( bts::vote::exception_message, (e) )
FC_REFLECT( bts::vote::identity_verification_request_message, (request) )
FC_REFLECT( bts::vote::identity_verification_response_message, (response) )
