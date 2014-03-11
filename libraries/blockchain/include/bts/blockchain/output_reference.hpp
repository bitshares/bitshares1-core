#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/city.hpp>
namespace bts { namespace blockchain {

/**
 *  A reference to a transaction and output index.
 */
struct output_reference
{
  output_reference():output_idx(0){}
  output_reference( const fc::uint160& trx, uint8_t idx )
  :trx_hash(trx),output_idx(idx){}
  fc::uint160  trx_hash;   // the hash of a transaction, TODO: switch to trx_id_type typedef rather than uint160 directly
  uint8_t  output_idx; // the output index in the transaction trx_hash
  
  friend bool operator==( const output_reference& a, const output_reference& b )
  {
     return a.trx_hash == b.trx_hash && a.output_idx == b.output_idx;
  }
  friend bool operator!=( const output_reference& a, const output_reference& b )
  {
     return !(a==b);
  }
  friend bool operator < ( const output_reference& a, const output_reference& b )
  {
     return a.trx_hash == b.trx_hash ? a.output_idx < b.output_idx : a.trx_hash < b.trx_hash;
  }
};
}} 

#include <unordered_map>
namespace std {
  /**
   *  This is implemented to facilitate generation of unique
   *  sets of inputs.
   */
  template<>
  struct hash<bts::blockchain::output_reference>
  {
     size_t operator()( const bts::blockchain::output_reference& e )const
     {
        return fc::city_hash64( (char*)&e.trx_hash, sizeof(e.trx_hash) )^e.output_idx;
     }
  };

} // std
