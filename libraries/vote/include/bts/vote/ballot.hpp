#pragma once

namespace bts { namespace vote {

   using bts::blockchain;

   /**
    *  An identity is tied to a private key, which is
    *  represented by the hash of its public key.  Each identity
    *  has many properties that can be attributed to it such as
    *  name, birth date, address, ssn, etc.  Each of these properties
    *  must be individually attributed to a particular private key.
    *
    *  These properties allow arbitrary named data to be attributed
    *  to an identity.  A salt is included to prevent brute forcing
    *  the value given a hash.  
    */
   struct identity_property
   {
      digest_type    id()const;

      address        identity;
      fc::uint128    salt;
      string         name;
      variant        value;
   };

   /**
    *  When someone signs a document that signature is only valid for
    *  a limited time range.  Absent a blockchain to retract a signature the
    *  time range prevents a need for renewal.
    */
   struct signature_data
   {
      /** sets the signature field as signer.sign( digest( identity_property_id ) ) */
      void sign( const private_key_type& signer, 
                 const digest_type& identity_property_id );

      /**  hash( id + valid_from + valid_until ) ) */
      digest_type digest( const digest_type& id)const;

      /** recovers public key of signature given identity property id */
      public_key_type signer( const digest_type& id )const;

      time_point_sec valid_from;
      time_point_sec valid_until;
      signature_type signature;
   };

   /**
    * Each identity property can be signed by more than one 
    * identity provider.
    */
   struct signed_identity_property : public identity_property
   {
      vector<signature_data> verifier_signatures;
   };

   /**
    *  An identity is a collection of properties that share a common
    *  private key recognized as the address of the public key.
    */
   struct identity
   {
      digest_type digest()const;
      unordered_map<digest_type, signed_identity_property>          properties;
   };

   /**
    *  An identtiy signed by its owner.... ties everything together. 
    */
   struct signed_identity : public identity
   {
      public_key_type     get_public_key()const;
      digest_type         get_property_index_digest()const;
      identity_property   get_property( const string& name )const;
      // owner_signature = owner_key.sign( digest() )
      void                sign_by_owner( const private_key_type& owner_key );

      optional<signature_type> owner_signature;
   };

   /**
    * A ballot is cast in a database/blockchain and recognizes 
    * canidates by the hash of their name.
    */
   struct ballot
   {
      uint32_t    election_id = 0;
      uint64_t    canidate_id = 0;    // hash of canidates full name lower case.
      bool        approve     = true; // for approval voting
      time_point  date;

      digest_type digest()const;
   };

   /**
    *  For a ballot to be counted it must be signed by a voter who has had
    *  their pulbic key signed by one or more registrars.  The count will
    *  varry based upon how the ballots are interpreted (approval, winner takes all, etc),
    *  with one election we can have many alternative results simultaniously. 
    */
   struct signed_ballot : public ballot
   {
      /**
       *  The registrar's signature on the address(voter_public_key)
       */
       vector<signature_type>  registrars;  
       /** voters signature on ballot::digest() */
       signature_type          voter_signature;

       void                    sign( const private_key_type& voter_private_key );
       public_key_type         voter_public_key()const;
   };


   // wallet state protected by user password and should be encrypted when stored on disk.
   struct wallet_state
   {
        signed_identity  real_identity;
        private_key_type real_id_private_key;
        private_key_type voter_id_private_key;

        map<mixer_id, token_data>  token_state;// blined/unblinded tokens generated for this mixer.

        // signature of registrars on voter_id_public_key
        vector<signature_type>  registrars;  

        // trx history
        vector<signed_ballot>   cast_ballots;
   };



} } // bts::vote

FC_REFLECT( bts::vote::identity, (owner)(person_info_digest) )
FC_REFLECT_DERIVED( bts::vote::signed_identity, (bts::vote::identity), (verifier_signature)(owner_signature) )
FC_REFLECT( bts::vote::ballot, (election_id)(voter_id)(canidate_id)(registrars)(booth_signature)(date) )
