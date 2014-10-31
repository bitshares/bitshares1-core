#include <bts/vote/messages.hpp>

namespace bts { namespace vote {
const message_type exception_message::type = exception_message_type;
const message_type identity_verification_request_message::type = verification_request_message_type;
const message_type identity_verification_response_message::type = verification_response_message_type;
const message_type get_verification_message::type = get_verification_message_type;
} } // namespace bts::vote
