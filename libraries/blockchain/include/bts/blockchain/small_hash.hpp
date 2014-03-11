#pragma once
#include <fc/crypto/ripemd160.hpp>

namespace bts { namespace blockchain {
  typedef fc::ripemd160  uint160;

  /**
   *   Performs ripemd160(seed);
   */
  uint160 small_hash( const fc::sha512& seed );

  /**
   *  The goal of the small hash is to be secure using as few
   *  bits as possible. 
   *
   *  Performs  small_hash( sha512(data,len) )
   */
  uint160 small_hash( const char* data, size_t len );

} }
