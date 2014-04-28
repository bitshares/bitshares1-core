#pragma once

/** 
 *  @file bts/blockchain/outputs.hpp
 *  @brief defines output types used by blockchain 
 */
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/asset.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/io/varint.hpp>

namespace bts { namespace blockchain {

/**
 *  @enum claim_type_enum
 *  @brief Enumerates the types of supported claim types
 */
enum claim_type_enum
{
   /** basic claim by single address */
   null_claim_type      = 0,
   claim_by_pts         = 1, ///< someone importing a PTS balance
   claim_by_signature   = 2, ///< someone signs with an address
   claim_by_multi_sig   = 3, ///< N of M signatures required
   claim_by_password    = 4, ///< used for cross-chain trading
   claim_name           = 5, ///< used to register a name that is unique
   claim_fire_delegate  = 6, ///< used to fire a delegate with cause
   // 10->19 reserved for BitShares X
   // 20->29 reserved for BitShares DNS
};

//typedef fc::enum_type<fc::unsigned_int,claim_type_enum> claim_type;
typedef uint8_t claim_type;

/**
 *   An input that references an output that can be claimed by
 *   an address.  A compact signature is used that when combined
 *   with the hash of the transaction containing this input will
 *   allow the public key and therefore address to be discovered and
 *   validated against the output claim function.
 */
struct claim_by_signature_input 
{
   static const claim_type_enum type;
};

struct claim_by_signature_output
{
   static const claim_type_enum type;
   claim_by_signature_output( const address& a ):owner(a){}
   claim_by_signature_output(){}
   address  owner; // checksummed hash of public key
};

struct claim_by_pts_output
{
   static const claim_type_enum type;
   claim_by_pts_output( const pts_address& a ):owner(a){}
   claim_by_pts_output(){}
   pts_address  owner; // checksummed hash of public key
};

/**
 *  This output can be claimed if sighed by payer & payee or
 *  signed by payee and the proper password is provided.
 */
struct claim_by_password_output
{
    static const claim_type_enum type;
    address          payer;
    address          payee;
    fc::ripemd160    hashed_password;
};

/**
 *   Provide the password such that ripemd160( password ) == claim_by_password_output::hashed_password
 */
struct claim_by_password_input
{
    static const claim_type_enum type;
    fc::uint128     password; ///< random number generated for cross chain trading
};


/**
 *  Used for multi-sig transactions that require N of M addresses to sign before
 *  the output can be spent.
 */
struct claim_by_multi_sig_output
{
    static const claim_type_enum type;
    claim_by_multi_sig_output():required(1){}
    uint8_t              required;
    std::vector<address> addresses;
};

struct claim_by_multi_sig_input
{
    static const claim_type_enum type;
};

struct claim_name_output
{
    static const claim_type_enum type;
    
    /**
     * Valid names start with a-z, are all lower case, and may have -
     */
    static bool is_valid_name( const std::string& name );

    claim_name_output():delegate_id(0){}

    claim_name_output( std::string n, const fc::variant&, uint32_t did, fc::ecc::public_key own );

    std::string          name; ///< a valid name, must follow DNS naming conventions
    std::string          data; ///< a JSON String, must parse to be included.

    /**
     *  A value of 0 means that this registration is to reserve the name only, and
     *  not for the purpose of being voted on to be a potential delegate.  
     *
     *  A delegate by resign by updating their delegate_id to be 0 in which case
     *  all clients who voted for this delegate must reallocate their votes 
     *
     *  If delegate_id is not 0 then a registration fee is required equal to the
     *  average revenue from 100 blocks.
     */
    uint32_t             delegate_id; 

    /**
     *  Owner of the name / delegate_id, this is the master key that will likely
     *  remain in cold-storage.
     */
    fc::ecc::public_key  owner;

    /**
     * This is the active key for this owner, that is not in cold storage and can
     * be used for signing messages.  
     */
    fc::ecc::public_key  active;
};

enum fire_reason
{
     /** produce a transaction that the delegate reported incorrectly 
      * at the time the signed_certified_transaction was signed. 
      *
      * @see bts::blockchain::signed_certified_transaction
      */
     invalid_transaction_cert   = 0,

     /**
      * produce two block headers for the same timeslot signed
      * by the same delegate.
      *
      * @see bts::blockchain::signed_header_conflict
      */
     duplicate_block_signatures = 1
};

struct claim_fire_delegate_output
{
    static const claim_type_enum type;

    claim_fire_delegate_output():reason(0){}
    claim_fire_delegate_output(uint8_t r, std::vector<char> p):reason(r),proof(std::move(p)){}

    uint8_t           reason;
    std::vector<char> proof;
};


} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::claim_type_enum, 
    (null_claim_type)
    (claim_by_pts)
    (claim_by_signature)
    (claim_by_multi_sig)
    (claim_by_password)
    )

FC_REFLECT( bts::blockchain::claim_by_signature_output, (owner) )
FC_REFLECT( bts::blockchain::claim_by_pts_output, (owner) )
FC_REFLECT( bts::blockchain::claim_by_multi_sig_output, (required)(addresses) )
FC_REFLECT( bts::blockchain::claim_by_password_output, (payer)(payee)(hashed_password) )
FC_REFLECT( bts::blockchain::claim_name_output, (name)(data)(delegate_id)(owner)(active) )
FC_REFLECT( bts::blockchain::claim_fire_delegate_output,  (reason)(proof)  )

FC_REFLECT( bts::blockchain::claim_by_signature_input,    BOOST_PP_SEQ_NIL )
FC_REFLECT( bts::blockchain::claim_by_password_input,     (password)       )

