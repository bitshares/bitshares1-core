#pragma once

#include <fc/crypto/sha256.hpp>
#include <fc/time.hpp>

#include <bts/blockchain/types.hpp>

#include <string>
#include <vector>
#include <map>

namespace bts { namespace vote {
using bts::blockchain::address;
using bts::blockchain::public_key_type;
using bts::blockchain::private_key_type;
using fc::time_point_sec;
using fc::ecc::compact_signature;
using fc::variant;
using fc::optional;
using std::string;
using std::vector;
using std::unordered_map;
using std::map;

typedef fc::sha256 digest_type;

enum request_status_enum {
   awaiting_processing,
   in_processing,
   accepted,
   rejected
};

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
   void sign( const private_key_type& signer,
              const digest_type& identity_property_id )
   {
      signature = signer.sign_compact(digest(identity_property_id));
   }

   /**  hash( id + valid_from + valid_until ) ) */
   digest_type digest( const digest_type& id)const;

   /** Recovers public key of signature given identity property id.
     * Returns public_key_type() if current time is outside the validity window.
     */
   public_key_type signer( const digest_type& id )const;

   time_point_sec valid_from;
   time_point_sec valid_until;
   compact_signature signature;
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
 *  An identity is a collection of properties that belong to a common owner key
 */
struct identity
{
   digest_type digest()const;
   optional<identity_property> get_property( const string& name )const;

   address                                   owner;
   vector<signed_identity_property>          properties;
};

/**
 *  An identity signed by its owner.... ties everything together.
 */
struct signed_identity : public identity
{
   optional<public_key_type>     get_public_key()const
   {
      return owner_signature? public_key_type(fc::ecc::public_key(*owner_signature, digest()))
                            : optional<public_key_type>();
   }
   void                          sign_by_owner( const private_key_type& owner_key )
   {
      owner_signature = owner_key.sign_compact(digest());
   }

   optional<compact_signature> owner_signature;
};

/**
 * @brief A request from a user for ID verification
 *
 * The user fills in the names and salts for each property he wishes the verifier to fill and sign. He also provides a
 * set of photos of himself and his credentials which the verifier uses to determine this user as a unique, validated
 * identity.
 *
 * Photos are base64 JPEGs.
 */
struct identity_verification_request : public identity
{
   digest_type digest()const;

   string owner_photo;
   string id_front_photo;
   string id_back_photo;
   string voter_reg_photo;
};

/**
 * @brief A summary of an identity verification request
 *
 * The objective is to keep this struct small (several kilobytes) as this is the in-memory record of an identity request
 */
struct identity_verification_request_summary : public identity {
   identity_verification_request_summary()
       :timestamp(fc::time_point::now())
   {}

   bool operator== (const identity_verification_request_summary& other) const {
      return status == other.status && timestamp == other.timestamp;
   }

   request_status_enum status = awaiting_processing;
   fc::time_point timestamp;
   fc::optional<string> rejection_reason;
};

/**
 * @brief A response to the ID verification request
 *
 * verified_identity will be set iff accepted == true
 */
struct identity_verification_response
{
   bool accepted = false;
   fc::optional<string> rejection_reason;
   fc::optional<identity> verified_identity;
};

/**
 * A ballot is cast in a database/blockchain and recognizes
 * candidates by the hash of their name.
 */
struct ballot
{
   uint32_t        election_id  = 0;
   uint64_t        candidate_id = 0;    // Hash of candidate's full name lower case.
   bool            approve      = true; // For approval voting
   time_point_sec  date;

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
   vector<compact_signature>  registrar_signatures;
   /** voters signature on ballot::digest() */
   compact_signature          voter_signature;

   void                    sign( const private_key_type& voter_private_key )
   {
      voter_signature = voter_private_key.sign_compact(digest());
   }
   public_key_type         voter_public_key()const
   {
      return fc::ecc::public_key(voter_signature, digest());
   }
};


// wallet state protected by user password and should be encrypted when stored on disk.
struct wallet_state
{
     signed_identity  real_identity;
     private_key_type real_id_private_key;
     private_key_type voter_id_private_key;

     //Need to define token_data...
//     map<registrar_id, token_data>  token_state; // blinded/unblinded tokens generated for this registrar.

     // signature of registrars on voter_id_public_key
     vector<compact_signature>  registrars;

     // trx history
     vector<signed_ballot>   cast_ballots;
};

} } // bts::vote

FC_REFLECT( bts::vote::signature_data, (valid_from)(valid_until)(signature) )
FC_REFLECT( bts::vote::identity_property, (identity)(salt)(name)(value) )
FC_REFLECT_DERIVED( bts::vote::signed_identity_property, (bts::vote::identity_property), (verifier_signatures) )
FC_REFLECT( bts::vote::identity, (owner)(properties) )
FC_REFLECT_DERIVED( bts::vote::signed_identity, (bts::vote::identity), (owner_signature) )
FC_REFLECT_DERIVED( bts::vote::identity_verification_request, (bts::vote::identity),
                    (owner_photo)(id_front_photo)(id_back_photo)(voter_reg_photo) )
FC_REFLECT_DERIVED( bts::vote::identity_verification_request_summary, (bts::vote::identity),
                    (status)(timestamp)(rejection_reason) )
FC_REFLECT( bts::vote::ballot, (election_id)(candidate_id)(approve)(date) )
FC_REFLECT_DERIVED( bts::vote::signed_ballot, (bts::vote::ballot), (registrar_signatures)(voter_signature) )
FC_REFLECT_TYPENAME( bts::vote::request_status_enum )
FC_REFLECT_ENUM( bts::vote::request_status_enum,
                 (awaiting_processing)(in_processing)(accepted)(rejected) )
