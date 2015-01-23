#include <bts/blockchain/address.hpp>
#include <bts/blockchain/withdraw_types.hpp>

#include <fc/crypto/base58.hpp>
#include <algorithm>

namespace bts {
  namespace blockchain {
   address::address(){}

   address::address( const std::string& base58str )
   {
      FC_ASSERT( is_valid( base58str ) );
      std::string prefix( BTS_ADDRESS_PREFIX );
      if( is_valid( base58str, "BTSX" ) ) prefix = std::string( "BTSX" );
      std::vector<char> v = fc::from_base58( base58str.substr( prefix.size() ) );
      memcpy( (char*)addr._hash, v.data(), std::min<size_t>( v.size()-4, sizeof( addr ) ) );
   }
   address::address( const withdraw_condition& condition )
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, condition );
      addr = fc::ripemd160::hash( enc.result() );
   }

   bool address::is_valid( const std::string& base58str, const std::string& prefix )
   {
      if( prefix == BTS_ADDRESS_PREFIX && is_valid( base58str, "BTSX" ) )
          return true;

      const size_t prefix_len = prefix.size();
      if( base58str.size() <= prefix_len )
          return false;
      if( base58str.substr( 0, prefix_len ) != prefix )
          return false;
      std::vector<char> v;
      try
      {
		  v = fc::from_base58( base58str.substr( prefix_len ) );
	  }
	  catch( const fc::parse_error_exception& e )
	  {
		  return false;
	  }
      if( v.size() != sizeof( fc::ripemd160 ) + 4 )
          return false;
      const fc::ripemd160 checksum = fc::ripemd160::hash( v.data(), v.size() - 4 );
      if( memcmp( v.data() + 20, (char*)checksum._hash, 4 ) != 0 )
          return false;
      return true;
   }

   address::address( const fc::ecc::public_key& pub )
   {
       auto dat = pub.serialize();
       addr = fc::ripemd160::hash( fc::sha512::hash( dat.data, sizeof( dat ) ) );
   }

   address::address( const pts_address& ptsaddr )
   {
       addr = fc::ripemd160::hash( (char*)&ptsaddr, sizeof( ptsaddr ) );
   }

   address::address( const fc::ecc::public_key_data& pub )
   {
       addr = fc::ripemd160::hash( fc::sha512::hash( pub.data, sizeof( pub ) ) );
   }

   address::address( const bts::blockchain::public_key_type& pub )
   {
       addr = fc::ripemd160::hash( fc::sha512::hash( pub.key_data.data, sizeof( pub.key_data ) ) );
   }

   address::operator std::string()const
   {
        fc::array<char,24> bin_addr;
        memcpy( (char*)&bin_addr, (char*)&addr, sizeof( addr ) );
        auto checksum = fc::ripemd160::hash( (char*)&addr, sizeof( addr ) );
        memcpy( ((char*)&bin_addr)+20, (char*)&checksum._hash[0], 4 );
        return BTS_ADDRESS_PREFIX + fc::to_base58( bin_addr.data, sizeof( bin_addr ) );
   }

} } // namespace bts::blockchain

namespace fc
{
    void to_variant( const bts::blockchain::address& var,  variant& vo )
    {
        vo = std::string(var);
    }
    void from_variant( const variant& var,  bts::blockchain::address& vo )
    {
        vo = bts::blockchain::address( var.as_string() );
    }
}
