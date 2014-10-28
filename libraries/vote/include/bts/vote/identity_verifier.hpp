#pragma once

#include <bts/vote/types.hpp>

#include <fc/filesystem.hpp>

namespace bts { namespace vote {
namespace detail { class identity_verifier_impl; }

class identity_verifier {
   std::shared_ptr<detail::identity_verifier_impl> my;

public:
   identity_verifier();
   ~identity_verifier();

   void open(fc::path data_dir);
   void close();
   bool is_open();

   void verify_identity(const identity_verification_request& request, const fc::ecc::compact_signature& signature);
};
} } // namespace bts::vote
