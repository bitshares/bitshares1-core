#pragma once

#include <fc/exception/exception.hpp>

namespace bts { namespace mail {
    FC_DECLARE_EXCEPTION(mail_exception, 70000, "Mail Exception");
    FC_DECLARE_DERIVED_EXCEPTION(timestamp_too_old, mail_exception, 70001, "timestamp too old");
    FC_DECLARE_DERIVED_EXCEPTION(timestamp_in_future, mail_exception, 70002, "timestamp in future");
    FC_DECLARE_DERIVED_EXCEPTION(invalid_proof_of_work, mail_exception, 70003, "invalid proof-of-work");
    FC_DECLARE_DERIVED_EXCEPTION(message_too_large, mail_exception, 70004, "message too large");
    FC_DECLARE_DERIVED_EXCEPTION(message_already_stored, mail_exception, 70005, "message already stored");
} } // namespace bts::mail
