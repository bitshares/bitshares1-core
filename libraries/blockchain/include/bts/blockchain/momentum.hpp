#pragma once
#include <fc/array.hpp>
#include <fc/io/varint.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/reflect/reflect.hpp>

#define MOMENTUM_NONCE_BITS 26
#define MAX_MOMENTUM_NONCE  (1<<MOMENTUM_NONCE_BITS)

namespace bts { namespace blockchain {
   typedef fc::sha256     pow_seed_type;
   typedef fc::ripemd160  pow_hash_type;

   /** 
    *  @return all collisions found in the nonce search space 
    */
   std::vector< std::pair<uint32_t,uint32_t> > momentum_search( pow_seed_type head );
   bool momentum_verify( pow_seed_type head, uint32_t a, uint32_t b );

} } // bts::blockchain

