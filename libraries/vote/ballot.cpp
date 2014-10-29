#include <bts/vote/ballot.hpp>
#include <fc/crypto/digest.hpp>

namespace bts { namespace vote {

   digest_type identity_property::id()const { return fc::digest(*this);
   }

   digest_type signature_data::digest( const digest_type& id )const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, id );
      fc::raw::pack( enc, valid_from );
      fc::raw::pack( enc, valid_until );
      return enc.result();
   }

   public_key_type signature_data::signer( const digest_type& id )const
   {
      return fc::ecc::public_key( signature, digest(id) );
   }

   digest_type identity::digest()const { return fc::digest( *this ); }

   optional<public_key_type> signed_identity::get_public_key()const
   {
      if( !owner_signature ) return optional<public_key_type>();
      return fc::ecc::public_key( *owner_signature, digest() );
   }

   optional<signed_identity_property>  signed_identity::get_property( const string& name )const
   {
      for( const auto& prop : properties )
         if( prop.name == name )
            return prop;
      return optional<signed_identity_property>();
   }

   void signed_identity::sign_by_owner( const private_key_type& owner_key )const
   {
      owner_signature = owner_key.sign( fc::digest( *this ) );
   }

} } 
