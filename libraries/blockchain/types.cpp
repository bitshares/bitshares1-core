#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/varint.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

#include <bts/blockchain/types.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/address.hpp>


namespace bts { namespace blockchain {

    using std::string;   
 
    public_key_type::public_key_type():key_data(){};

    public_key_type::public_key_type( const fc::ecc::public_key_data& data )
        :key_data( data ) {};

    public_key_type::public_key_type( const fc::ecc::public_key& pubkey )
        :key_data( pubkey ) {};

    public_key_type::public_key_type( const std::string& base58str )
    {
       static const size_t prefix_len = strlen(BTS_ADDRESS_PREFIX);
       FC_ASSERT( base58str.size() > prefix_len );
       FC_ASSERT( base58str.substr( 0, prefix_len ) == BTS_ADDRESS_PREFIX, "", ("base58str", base58str) );
       auto bin = fc::from_base58( base58str.substr( prefix_len ) );
       auto bin_key = fc::raw::unpack<binary_key>(bin);
       key_data = bin_key.data;
       FC_ASSERT( fc::ripemd160::hash( key_data.data, key_data.size() )._hash[0] == bin_key.check );
    };

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


}} //bts::blockchain


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
} //fc
