#pragma once

#include <bts/blockchain/address.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>

#include <functional>

namespace fc
{
    class path;
    class microseconds;
    class time_point;
    class time_point_sec;
}

namespace bts { namespace blockchain {

    typedef fc::ripemd160               block_id_type;
    typedef fc::ripemd160               transaction_id_type;
    typedef fc::ripemd160               public_key_hash_type;
    typedef fc::ripemd160               secret_hash_type;
    typedef fc::ripemd160               order_id_type;
    typedef fc::sha256                  digest_type;
    typedef fc::ecc::compact_signature  signature_type;
    typedef fc::ecc::private_key        private_key_type;
    typedef address                     balance_id_type;
    typedef fc::signed_int              asset_id_type;
    typedef fc::signed_int              account_id_type;
    typedef int64_t                     share_type;
    typedef uint64_t                    slate_id_type;

    using std::string;
    using std::function;
    using fc::variant;
    using fc::variant_object;
    using fc::mutable_variant_object;
    using fc::optional;
    using std::map;
    using std::unordered_map;
    using std::set;
    using std::unordered_set;
    using std::vector;
    using std::pair;
    using fc::path;
    using fc::sha512;
    using fc::sha256;
    using std::unique_ptr;
    using std::shared_ptr;
    using fc::time_point_sec;
    using fc::time_point;
    using fc::microseconds;
    using fc::unsigned_int;
    using fc::signed_int;

    struct public_key_type
    {
        struct binary_key
        {
            binary_key():check(0){}
            uint32_t                 check;
            fc::ecc::public_key_data data;
        };

        fc::ecc::public_key_data key_data;

        public_key_type();
        public_key_type( const fc::ecc::public_key_data& data );
        public_key_type( const fc::ecc::public_key& pubkey );
        explicit public_key_type( const std::string& base58str );
        operator fc::ecc::public_key_data() const;
        operator fc::ecc::public_key() const;
        explicit operator std::string() const;
        friend bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2);
        friend bool operator == ( const public_key_type& p1, const public_key_type& p2);
        friend bool operator != ( const public_key_type& p1, const public_key_type& p2);

        bool is_valid_v1( const std::string& base58str );
    };

} } // bts::blockchain

namespace fc
{
    void to_variant( const bts::blockchain::public_key_type& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  bts::blockchain::public_key_type& vo );
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::public_key_type, (key_data) )
FC_REFLECT( bts::blockchain::public_key_type::binary_key, (data)(check) );
