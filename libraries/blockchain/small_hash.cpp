#include <bts/blockchain/small_hash.hpp>
#include <fc/crypto/sha512.hpp>

namespace bts { namespace blockchain {
  typedef fc::ripemd160  uint160;

  /**
   *   Performs ripemd160(seed);
   */
  uint160 small_hash( const fc::sha512& seed )
  {
     return fc::ripemd160::hash(seed);
  }

  /**
   *  The goal of the small hash is to be secure using as few
   *  bits as possible. 
   *
   *  Performs  small_hash( sha512(data,len) )
   */
  uint160 small_hash( const char* data, size_t len )
  {
     return small_hash( fc::sha512::hash(data,len) );
  }

} } // namespace blockchain
