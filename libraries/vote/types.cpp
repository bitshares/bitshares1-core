#include <bts/vote/types.hpp>

namespace bts { namespace vote {

digest_type identity_property::digest()const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, *this);
   return enc.result();
}

digest_type signature_data::digest(const digest_type &id) const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, id);
   fc::raw::pack(enc, valid_from);
   fc::raw::pack(enc, valid_until);
   return enc.result();
}

public_key_type signature_data::signer(const digest_type &id) const
{
   fc::time_point_sec now = fc::time_point::now();
   if( now < valid_from || now > valid_until )
      return public_key_type();
   return fc::ecc::public_key(signature, digest(id));
}

digest_type identity::digest() const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, *this);
   return enc.result();
}

digest_type ballot::digest() const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, *this);
   return enc.result();
}
} } // namespace bts::vote
