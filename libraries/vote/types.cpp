#include <bts/vote/types.hpp>

#include <fc/crypto/digest.hpp>

namespace bts { namespace vote {

digest_type identity_property::id()const
{
   return fc::digest(*this);
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
   return fc::digest(*this);
}

optional<identity_property> identity::get_property(const string& name) const
{
   for( const signed_identity_property& property : properties )
      if( property.name == name )
         return property;
   return optional<identity_property>();
}

digest_type ballot::digest() const
{
   return fc::digest(*this);
}

digest_type identity_verification_request::digest() const
{
   return fc::digest(*this);
}

} } // namespace bts::vote
