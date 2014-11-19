#include <bts/vote/types.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace vote {

identity_property identity_property::generate(string name, const variant& value)
{
   uint64_t salt1 = *((uint64_t*)(fc::ecc::private_key::generate().get_secret().data()));
   uint64_t salt2 = *((uint64_t*)(fc::ecc::private_key::generate().get_secret().data()));
   return identity_property({fc::uint128(salt1, salt2), name, value});
}

digest_type identity_property::id(const blockchain::address& identity)const
{
   return fc::digest(*this);
}

expiring_signature expiring_signature::sign(const blockchain::private_key_type& signer,
                                    const digest_type& digest,
                                    fc::time_point_sec valid_from,
                                    fc::time_point_sec valid_until)
{
   expiring_signature data;
   data.valid_from = valid_from;
   data.valid_until = valid_until;
   data.signature = signer.sign_compact(data.digest(digest));
   return data;
}

digest_type expiring_signature::digest(const digest_type &id) const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, id);
   fc::raw::pack(enc, valid_from);
   fc::raw::pack(enc, valid_until);
   return enc.result();
}

public_key_type expiring_signature::signer(const digest_type &id) const
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

optional<signed_identity_property> identity::get_property(const string& name) const
{
   optional<signed_identity_property> result;
   for( const signed_identity_property& property : properties )
      if( property.name == name &&
          (!result || result->verifier_signatures.size() < property.verifier_signatures.size()) )
         result = property;
   return result;
}

void identity::sign(const blockchain::private_key_type& key,
                    fc::time_point_sec valid_from,
                    fc::time_point_sec valid_until)
{
   for( signed_identity_property& property : properties )
      property.verifier_signatures.push_back(expiring_signature::sign(key, property.id(owner), valid_from, valid_until));
}

digest_type identity_verification_request::digest() const
{
   return fc::digest(*this);
}

expiring_signature ballot::authorize_voter(const digest_type& ballot_id,
                                       const fc::ecc::private_key& registrar_private_key,
                                       const blockchain::public_key_type& voter_public_key,
                                       fc::time_point_sec valid_from, fc::time_point_sec valid_until)
{
   fc::sha256::encoder enc;
   fc::raw::pack(enc, voter_public_key);
   fc::raw::pack(enc, ballot_id);
   return expiring_signature::sign(registrar_private_key, enc.result(), valid_from, valid_until);
}

bts::blockchain::public_key_type ballot::get_authorizing_registrar(
      const digest_type& ballot_id,
      const expiring_signature& authorization,
      const bts::blockchain::public_key_type& voter_public_key)
{
   fc::sha256::encoder enc;
   fc::raw::pack(enc, voter_public_key);
   fc::raw::pack(enc, ballot_id);
   return authorization.signer(enc.result());
}

} } // namespace bts::vote
