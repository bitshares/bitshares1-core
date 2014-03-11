#pragma once
#include <fc/crypto/bigint.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/ripemd160.hpp>

namespace bts { namespace blockchain {
  
  /**
   *  Calculates the difficulty of hash as 
   *
   *  (max224() / hash) >> 24 
   */
  uint64_t difficulty( const fc::sha224& hash_value );

  /** return 2^224 -1 */
  const fc::bigint&  max224();

  /**
   *  Calculates the difficulty of hash as 
   *
   *  (max160() / hash)
   */
  uint64_t difficulty( const fc::uint160& hash_value );

  /** return 2^160 -1 */
  const fc::bigint&  max160();

} } // namespace bts::blockchain
