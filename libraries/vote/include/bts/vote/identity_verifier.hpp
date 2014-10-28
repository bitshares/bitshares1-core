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

   void store_pending_identity(const public_key_type& owner_key,
                               const string& owner_photo,
                               const string& id_front_photo,
                               const string& id_back_photo,
                               const string& voter_reg_photo);
};
} } // namespace bts::vote
