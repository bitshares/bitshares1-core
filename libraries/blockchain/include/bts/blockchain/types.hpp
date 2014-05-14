#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/account.hpp>
#include <fc/io/varint.hpp>

namespace bts { namespace blockchain {

   typedef fc::ripemd160                    block_id_type;
   typedef fc::ripemd160                    transaction_id_type;
   typedef fc::ripemd160                    public_key_hash_type;
   typedef fc::sha256                       digest_type;
   typedef std::vector<transaction_id_type> transaction_ids;
   typedef fc::ecc::compact_signature       signature_type;
   typedef fc::ecc::public_key_data         public_key_type;
   typedef fc::ecc::private_key             private_key_type;
   typedef account                          account_id_type;
   typedef fc::signed_int                   asset_id_type;
   typedef fc::signed_int                   name_id_type;
   typedef uint32_t                         tapos_type; 
   typedef int64_t                          share_type;
   typedef int64_t                          bip_type;

   #define BASE_ASSET_ID  (asset_id_type())

} } // bts::blockchain
