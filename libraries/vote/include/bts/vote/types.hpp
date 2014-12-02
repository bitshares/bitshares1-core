#pragma once

#include <fc/crypto/digest.hpp>
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
   needs_further_review,
   in_processing,
   accepted,
   rejected,
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
   static identity_property generate(std::string name, const fc::variant& value = fc::variant());

   digest_type    id(const address& identity)const;

   bool operator==(const identity_property& other)const
   {
      return salt == other.salt && name == other.name && (value.is_null() ||
                                                          other.value.is_null() ||
                                                          value.as_string() == other.value.as_string());
   }

   fc::uint128    salt;
   string         name;
   variant        value;
};

/**
 *  When someone signs a document that signature is only valid for
 *  a limited time range. Absent a blockchain to retract a signature the
 *  time range prevents a need for renewal.
 */
struct expiring_signature
{
   static expiring_signature sign( const private_key_type& signer,
                               const digest_type& digest,
                               fc::time_point_sec valid_from,
                               fc::time_point_sec valid_until );

   /**  hash( id + valid_from + valid_until ) */
   digest_type digest( const digest_type& id)const;

   /** Recovers public key of signature given identity property id.
     * Returns public_key_type() if current time is outside the validity window.
     */
   public_key_type signer( const digest_type& id )const;

   /// Note that signatures are not deterministic, so the signer and dates could match, but this only returns true if
   /// the signatures are exactly identical.
   bool operator== (const expiring_signature& other)const
   {
      return valid_from == other.valid_from && valid_until == other.valid_until && signature == other.signature;
   }

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
   signed_identity_property(){}
   signed_identity_property(const identity_property& prop)
      : identity_property(prop){}
   signed_identity_property(identity_property&& prop)
      : identity_property(prop){}

   bool operator== (const signed_identity_property& other) const
   {
      return ((identity_property&)*this).operator ==(other) && verifier_signatures == other.verifier_signatures;
   }

   vector<expiring_signature> verifier_signatures;
};

/**
 *  An identity is a collection of properties that belong to a common owner key
 */
struct identity
{
   digest_type digest()const;
   optional<signed_identity_property> get_property( const string& name )const;
   void sign(const private_key_type& key, fc::time_point_sec valid_from, fc::time_point_sec valid_until);

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
   ///Clients can ignore this field.
   fc::optional<fc::microseconds> id;
};

/**
 * @brief A summary of an identity verification request
 *
 * The objective is to keep this struct small (several kilobytes) as this is the in-memory record of an identity request
 */
struct identity_verification_request_summary : public identity {
   identity_verification_request_summary()
       :id(fc::time_point::now().time_since_epoch())
   {}

   bool operator== (const identity_verification_request_summary& other) const {
      return status == other.status && id == other.id;
   }

   request_status_enum status = awaiting_processing;
   fc::microseconds id;
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
   fc::optional<time_point_sec> expiration_date;
   fc::optional<bool> owner_photo_valid;
   fc::optional<bool> id_front_photo_valid;
   fc::optional<bool> id_back_photo_valid;
   fc::optional<bool> voter_reg_photo_valid;
};

struct contestant
{
   string name;
   string description;
};

/**
 * @brief A contest is one of the questions on which voters may specify an opinion.
 *
 * A contest has a description and a list of free-form tags which are used to specify details about that particular
 * contest (what type of contest is it, what district it pertains to, hints on how to render it in a GUI, etc. etc.).
 * Also, a contest has some list of contestants which voters may specify opinions on.
 *
 * This format of representation was chosen to provide the utmost flexibility in what can be represented unambiguously
 * on the blockchain. This is because real-world contests are often defined in off-the-cuff, unstructured ways and the
 * blockchain must be capable of storing this data, and a UI needs to be able to display it in a sane format. A
 * free-form list of tags allows arbitrary contest types to be defined as needed, and provides hints for GUIs to render
 * them elegantly.
 */
struct contest
{
   string description;
   map<string,string> tags;
   ///Contestants are identified in decisions by their index in this vector
   vector<contestant> contestants;

   digest_type id()const
   {
      return fc::digest(*this);
   }
};

/**
 * @brief A ballot is a list of contests which a particular voter may vote for.
 *
 * A voter is assigned a ballot by the ID verifier based on his geographical location.
 */
struct ballot
{
   string title;
   string description;
   vector<digest_type> contests;

   digest_type id()const
   {
      return fc::digest(*this);
   }
   static expiring_signature authorize_voter(const digest_type& ballot_id,
                                         const fc::ecc::private_key& registrar_private_key,
                                         const public_key_type& voter_public_key,
                                         fc::time_point_sec valid_from, fc::time_point_sec valid_until);
   static public_key_type get_authorizing_registrar(const digest_type& ballot_id,
                                                    const expiring_signature& authorization,
                                                    const public_key_type& voter_public_key);
};

/**
 * @brief A voter_decision represents a vote on a particular contest.
 *
 * When a voter votes on a particular contest, he specifies opinions on some subset of the contestants, and potentially
 * some write-in contestants. A voter may only specify one opinion per contestant. Only the last valid decision per
 * voter per contest should be counted.
 *
 * Each opinion is comprised of a contestant ID and a number. The contestant ID is the index of the contestant in the
 * contest structure above. Write-in contestant IDs are the sum of the size of the list of official contestants and the
 * index of the desired write-in contestant in the vector write_in_names, thus if there are 4 official contestants, the
 * first contestant in write_in_names would be ID 4; the next would be ID 5, etc.
 */
struct voter_decision
{
   digest_type contest_id;
   vector<string> write_in_names;
   map<fc::signed_int, fc::signed_int> voter_opinions;
   digest_type ballot_id;
   vector<expiring_signature> registrar_authorizations;

   digest_type digest()const
   {
      return fc::digest(*this);
   }
};

/**
 * @brief A cast_decision is a voter_decision as returned by the ballot box API.
 *
 * The voting booth creates signed_voter_decision structs and submits them to be counted. When indexed off of the
 * blockchain, however, there is more data which should be recorded about a decision, such as its timestamp. This extra
 * data is made available via the cast_decision structure. Furthermore, some data from the signed_voter_decision is no
 * longer necessary, such as the signatures.
 */
struct cast_decision
{
   digest_type decision_id;
   digest_type contest_id;
   digest_type ballot_id;
   vector<string> write_in_names;
   map<fc::signed_int, fc::signed_int> voter_opinions;
   bts::blockchain::address voter_id;
   fc::time_point timestamp;
   bool authoritative;
   bool latest;
};

struct signed_voter_decision : public voter_decision
{
   fc::ecc::compact_signature voter_signature;

   signed_voter_decision(){}
   signed_voter_decision(const voter_decision& decision, const bts::blockchain::private_key_type& signing_key)
      : voter_decision(decision),
        voter_signature(signing_key.sign_compact(digest()))
   {}
   bts::blockchain::public_key_type voter_public_key()const
   {
      try {
         return fc::ecc::public_key(voter_signature, digest());
      } catch (...) {
         return fc::ecc::public_key();
      }
   }
};

// wallet state protected by user password and should be encrypted when stored on disk.
struct wallet_state
{
     signed_identity  real_identity;
     private_key_type real_id_private_key;
     private_key_type voter_id_private_key;

     // signature of registrars authorizing voter to vote on ballot
     vector<expiring_signature>  registrars;
};

} } // bts::vote

FC_REFLECT( bts::vote::expiring_signature, (valid_from)(valid_until)(signature) )
FC_REFLECT( bts::vote::identity_property, (salt)(name)(value) )
FC_REFLECT_DERIVED( bts::vote::signed_identity_property, (bts::vote::identity_property), (verifier_signatures) )
FC_REFLECT( bts::vote::identity, (owner)(properties) )
FC_REFLECT_DERIVED( bts::vote::signed_identity, (bts::vote::identity), (owner_signature) )
FC_REFLECT_DERIVED( bts::vote::identity_verification_request, (bts::vote::identity),
                    (owner_photo)(id_front_photo)(id_back_photo)(voter_reg_photo)(id) )
FC_REFLECT_DERIVED( bts::vote::identity_verification_request_summary, (bts::vote::identity),
                    (status)(id)(rejection_reason) )
FC_REFLECT( bts::vote::identity_verification_response, (accepted)
            (rejection_reason)(verified_identity)(expiration_date)
            (owner_photo_valid)(id_front_photo_valid)(id_back_photo_valid)(voter_reg_photo_valid) )
FC_REFLECT_TYPENAME( bts::vote::request_status_enum )
FC_REFLECT_ENUM( bts::vote::request_status_enum,
                 (awaiting_processing)(needs_further_review)(in_processing)(accepted)(rejected) )
FC_REFLECT( bts::vote::contestant, (name)(description) )
FC_REFLECT( bts::vote::contest, (description)(tags)(contestants) )
FC_REFLECT( bts::vote::ballot, (title)(description)(contests) )
FC_REFLECT( bts::vote::voter_decision, (contest_id)(ballot_id)(write_in_names)(voter_opinions) )
FC_REFLECT_DERIVED( bts::vote::signed_voter_decision, (bts::vote::voter_decision), (voter_signature) )
FC_REFLECT( bts::vote::cast_decision, (decision_id)(contest_id)(ballot_id)(write_in_names)(voter_opinions)(voter_id)
            (timestamp)(authoritative)(latest) )
