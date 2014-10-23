#pragma once

namespace bts { namespace vote {

   using bts::blockchain;

   struct identity
   {
      address       owner;
      digest_type   person_info_digest;
   };

   struct signed_identity : public identity
   {
      optional<signature_type> verifier_signature;
      optional<signature_type> owner_signature;
   };

   struct ballot
   {
      uint32_t election_id = 0;
      address  voter_id;

      uint64_t canidate_id = 0;  // hash of canidates full name lower case.
      /**
       *  The registrar's signature on the voter_id
       */
      vector<signature_type>  registrars; 
      time_point              date;
   };

   struct signed_ballot : public ballot
   {
       signature_type            voter_signature;

       bool validate_voter_signature();
   };

   /** a voting booth signs the voters signature certifying its role
    * in producing the private key.
    */
   struct booth_signed_ballot : public signed_ballot
   {
       signature_type  booth_signature;
       bool validate_booth_signature();
   };

   /** a ballot that has been accepted by a ballot box server as
    * having been submitted and included in the official count.
    */
   struct accepted_ballot : public signed_ballot
   {
       signature_type box_signature;
   };

   // API calls
   //    // called by wallet GUI to generate a signed identity with new public key
   //    // wallet stores all identities created.
   //    signed_identity  create_identity( personal_info );
   //    
   //    // if the ballot has been verified return it, if not it may be pending or rejected
   //    // will look up owner and submit the signed identity and personal_info passed to create
   //    signed_identity request_validation( verifier_url, owner_id )
   //
   //    // this request may be repeated because the registrar should store the blinded token with the owner id.
   //    // the blinded token is signed by the owner_id so that the registrar can prove that the request was 
   //    // provided by the proper owner and no one else can claim it.
   //    signed_token    request_ballot_token( blinded_token, owner_id_signature )
   //
   //    // if unblinded token is valid then sign voter_id with registrar signature.
   //    signature_type  request_voter_id( voter_id, unblinded_token )
   //
   //    Called by Registrar after verifying personal info, returned to user.
   //
   //    signed_identity  sign_identity( signed_identity, variant personal_info ) 
   //
   //

} } // bts::vote

FC_REFLECT( bts::vote::identity, (owner)(person_info_digest) )
FC_REFLECT_DERIVED( bts::vote::signed_identity, (bts::vote::identity), (verifier_signature)(owner_signature) )
FC_REFLECT( bts::vote::ballot, (election_id)(voter_id)(canidate_id)(registrars)(booth_signature)(date) )
