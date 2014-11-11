#include <bts/vote/types.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace vote {

digest_type identity_property::id(const blockchain::address& identity)const
{
   return fc::digest(*this);
}

signature_data signature_data::sign(const blockchain::private_key_type& signer, const digest_type& identity_property_id, fc::time_point_sec valid_from, fc::time_point_sec valid_until)
{
   signature_data data;
   data.valid_from = valid_from;
   data.valid_until = valid_until;
   data.signature = signer.sign_compact(data.digest(identity_property_id));
   return data;
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

void identity::sign(const blockchain::private_key_type& key,
                    fc::time_point_sec valid_from,
                    fc::time_point_sec valid_until)
{
   for( signed_identity_property& property : properties )
      property.verifier_signatures.push_back(signature_data::sign(key, property.id(owner), valid_from, valid_until));
}

digest_type identity_verification_request::digest() const
{
   return fc::digest(*this);
}

fc::ecc::compact_signature ballot::authorize_voter(const blockchain::public_key_type& voter_public_key,
                                                   const fc::ecc::private_key& registrar_private_key) const
{
   fc::sha256::encoder enc;
   fc::raw::pack(enc, voter_public_key);
   fc::raw::pack(enc, id());
   return registrar_private_key.sign_compact(enc.result());
}

bts::blockchain::public_key_type ballot::get_authorizing_registrar(
      const fc::ecc::compact_signature& authorization,
      const bts::blockchain::public_key_type& voter_public_key) const
{
   fc::sha256::encoder enc;
   fc::raw::pack(enc, voter_public_key);
   fc::raw::pack(enc, id());
   return fc::ecc::public_key(authorization, enc.result());
}

} } // namespace bts::vote
