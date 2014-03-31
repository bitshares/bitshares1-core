#include <bts/blockchain/difficulty.hpp>
#include <fc/exception/exception.hpp>
#include <string.h>

namespace bts { namespace blockchain {

  const fc::bigint& max224()
  {
     static fc::bigint m = [](){ 
        char data[224/8+1];
        memset( data, 0xff, sizeof(data) );
        data[0] = 0;
        return fc::bigint(data,sizeof(data));
     }();
     return m;
  }

  const fc::bigint& max256()
  {
     static fc::bigint m = [](){ 
        char data[256/8+1];
        memset( data, 0xff, sizeof(data) );
        data[0] = 0;
        return fc::bigint(data,sizeof(data));
     }();
     return m;
  }

  uint64_t difficulty( const fc::sha224& hash_value )
  {
      if( hash_value == fc::sha224() ) return uint64_t(-1); // div by 0

      auto dif = max224() / fc::bigint( (char*)&hash_value, sizeof(hash_value) );
      int64_t tmp = dif.to_int64();
      // possible if hash_value starts with 1
      if( tmp < 0 ) tmp = 0;
      return tmp;
  }

  uint64_t difficulty( const fc::sha256& hash_value )
  {
      if( hash_value == fc::sha256() ) return uint64_t(-1); // div by 0

      auto dif = max256() / fc::bigint( (char*)&hash_value, sizeof(hash_value) );
      int64_t tmp = dif.to_int64();
      // possible if hash_value starts with 1
      if( tmp < 0 ) tmp = 0;
      return tmp;
  }

  const fc::bigint& max160()
  {
     static fc::bigint m = [](){ 
        char data[160/8+1];
        memset( data, 0xff, sizeof(data) );
        data[0] = 0;
        return fc::bigint(data,sizeof(data));
     }();
     return m;
  }

  uint64_t difficulty( const fc::uint160& hash_value )
  {
      if( hash_value == fc::uint160() ) return uint64_t(-1); // div by 0

      auto dif = max160() / fc::bigint( (char*)&hash_value, sizeof(hash_value) );
      int64_t tmp = dif.to_int64();
      // possible if hash_value starts with 1
      if( tmp < 0 ) tmp = 0;
      return tmp;
  }

} } // bts::blockchain
