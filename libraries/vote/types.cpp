#include <bts/vote/types.hpp>

namespace bts { namespace vote {
digest_type identity_property::digest()const
{
   fc::sha256::encoder enc;
   fc::raw::pack(enc, *this);
   return enc.result();
}
} } // namespace bts::vote
