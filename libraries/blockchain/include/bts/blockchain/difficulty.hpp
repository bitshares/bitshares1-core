#pragma once
#include <fc/crypto/bigint.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/ripemd160.hpp>

namespace bts { namespace blockchain {

    /** return 2^224 -1 */
    const fc::bigint&  max224();
    /** return 2^256 -1 */
    const fc::bigint&  max256();
    /** return 2^160 -1 */
    const fc::bigint&  max160();
  
    /**
     *  Calculates the difficulty of hash as 
     *
     *  (max224() / hash) >> 24 
     */
    uint64_t difficulty( const fc::sha224& hash_value );
    uint64_t difficulty( const fc::sha256& hash_value );
    /**
     *  Calculates the difficulty of hash as 
     *
     *  (max160() / hash)
     */
    uint64_t difficulty( const fc::uint160& hash_value );


} } // namespace bts::blockchain
