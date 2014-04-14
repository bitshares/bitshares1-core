#include <algorithm>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/exception/exception.hpp>

#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/small_hash.hpp>

namespace bts { namespace blockchain {
   pts_address::pts_address()
   {
      memset( addr.data, 0, sizeof(addr.data) );
   }

   pts_address::pts_address( const std::string& base58str )
   {
      std::vector<char> v = fc::from_base58( fc::string(base58str) );
      if( v.size() )
         memcpy( addr.data, v.data(), std::min<size_t>( v.size(), sizeof(addr) ) );

      if( !is_valid() )
      {
         FC_THROW_EXCEPTION( exception, "invalid pts_address ${a}", ("a", base58str) );  
      }
   }

   pts_address::pts_address( const fc::ecc::public_key& pub, bool compressed, uint8_t version )
   {
       fc::sha256 sha2;
       if( compressed )
       {
           auto dat = pub.serialize();
           sha2     = fc::sha256::hash(dat.data, sizeof(dat) );
       }
       else
       {
           auto dat = pub.serialize_ecc_point();
           sha2     = fc::sha256::hash(dat.data, sizeof(dat) );
       }
       auto rep      = fc::ripemd160::hash((char*)&sha2,sizeof(sha2));
       addr.data[0]  = version;
       memcpy( addr.data+1, (char*)&rep, sizeof(rep) );
       auto check    = fc::sha256::hash( addr.data, sizeof(rep)+1 );
       check = fc::sha256::hash(check); // double
       memcpy( addr.data+1+sizeof(rep), (char*)&check, 4 );
   }

   /**
    *  Checks the address to verify it has a 
    *  valid checksum
    */
   bool pts_address::is_valid()const
   {
//       if( addr.data[0]  != 56 ) return false;
       auto check    = fc::sha256::hash( addr.data, sizeof(fc::ripemd160)+1 );
       check = fc::sha256::hash(check); // double
       return memcmp( addr.data+1+sizeof(fc::ripemd160), (char*)&check, 4 ) == 0;
   }

   pts_address::operator std::string()const
   {
        return fc::to_base58( addr.data, sizeof(addr) );
   }


} } // namespace bts


namespace fc 
{ 
   void to_variant( const bts::blockchain::pts_address& var,  variant& vo )
   {
        vo = std::string(var);
   }
   void from_variant( const variant& var,  bts::blockchain::pts_address& vo )
   {
        vo = bts::blockchain::pts_address( var.as_string() );
   }
}
