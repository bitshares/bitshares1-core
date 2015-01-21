#include <bts/blockchain/config.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace blockchain {

    public_key_type::public_key_type():key_data(){};

    public_key_type::public_key_type( const fc::ecc::public_key_data& data )
        :key_data( data ) {};

    public_key_type::public_key_type( const fc::ecc::public_key& pubkey )
        :key_data( pubkey ) {};

    public_key_type::public_key_type( const std::string& base58str )
    {
	   // TODO:  Refactor syntactic checks into static is_valid()
	   //        to make public_key_type API more similar to address API
       std::string prefix( BTS_ADDRESS_PREFIX );
       try
       {
           if( is_valid_v1( base58str ) )
               prefix = std::string( "BTSX" );
       }
       catch( ... )
       {
       }
       const size_t prefix_len = prefix.size();
       FC_ASSERT( base58str.size() > prefix_len );
       FC_ASSERT( base58str.substr( 0, prefix_len ) ==  prefix , "", ("base58str", base58str) );
       auto bin = fc::from_base58( base58str.substr( prefix_len ) );
       auto bin_key = fc::raw::unpack<binary_key>(bin);
       key_data = bin_key.data;
       FC_ASSERT( fc::ripemd160::hash( key_data.data, key_data.size() )._hash[0] == bin_key.check );
    };

    bool public_key_type::is_valid_v1( const std::string& base58str )
    {
       std::string prefix( "BTSX" );
       const size_t prefix_len = prefix.size();
       FC_ASSERT( base58str.size() > prefix_len );
       FC_ASSERT( base58str.substr( 0, prefix_len ) ==  prefix , "", ("base58str", base58str) );
       auto bin = fc::from_base58( base58str.substr( prefix_len ) );
       auto bin_key = fc::raw::unpack<binary_key>(bin);
       fc::ecc::public_key_data key_data = bin_key.data;
       FC_ASSERT( fc::ripemd160::hash( key_data.data, key_data.size() )._hash[0] == bin_key.check );
       return true;
    }

    public_key_type::operator fc::ecc::public_key_data() const
    {
       return key_data;
    };

    public_key_type::operator fc::ecc::public_key() const
    {
       return fc::ecc::public_key( key_data );
    };

    public_key_type::operator std::string() const
    {
       binary_key k;
       k.data = key_data;
       k.check = fc::ripemd160::hash( k.data.data, k.data.size() )._hash[0];
       auto data = fc::raw::pack( k );
       return BTS_ADDRESS_PREFIX + fc::to_base58( data.data(), data.size() );
    }

    bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2)
    {
       return p1.key_data == p2.serialize();
    }

    bool operator == ( const public_key_type& p1, const public_key_type& p2)
    {
       return p1.key_data == p2.key_data;
    }

    bool operator != ( const public_key_type& p1, const public_key_type& p2)
    {
       return p1.key_data != p2.key_data;
    }

} } // bts::blockchain

namespace fc
{
    void to_variant( const bts::blockchain::public_key_type& var,  fc::variant& vo )
    {
        vo = std::string(var);
    }

    void from_variant( const fc::variant& var,  bts::blockchain::public_key_type& vo )
    {
        vo = bts::blockchain::public_key_type( var.as_string() );
    }
} // fc
