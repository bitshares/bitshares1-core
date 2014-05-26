#define BOOST_TEST_MODULE BitcoinWalletTest
#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/assert.hpp>
#include <fc/filesystem.hpp>

#include <bts/bitcoin/armory.hpp>
#include <bts/blockchain/pts_address.hpp>

BOOST_AUTO_TEST_CASE( import_armory_wallet_test_unencrypted )
{
    printf("Testing unecrypted wallet\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "tests/armory_pv6Mmjdh_decrypt.wallet" ), "" );
    printf("found %d keys\n", (int)keys.size());
    BOOST_REQUIRE( (keys.size() == 104) );

    std::vector<std::string> addrs;
    for (auto &key : keys) {
        bts::blockchain::pts_address a_c( key.get_public_key(), true );
        std::string addr = a_c;
        addrs.push_back( addr );
    }
    std::sort( addrs.begin(), addrs.end() );

    BOOST_REQUIRE( addrs[99] == "PvgkS8QwqEVGWVzFfnAaXUCWRX76RU3CfV" );
    BOOST_REQUIRE( addrs[96] == "PutWpxxTvUxNHXjDFmBf5stf6ATenQqzQ2" );
    BOOST_REQUIRE( addrs[77] == "PqT3Ub3jWg8fjvjjUo4xMAhB5MWKJB1i32" );
    BOOST_REQUIRE( addrs[34] == "PfxMkC5gUqGevh3v3sFHgCvu9pxYQUPYna" );
}

BOOST_AUTO_TEST_CASE( import_armory_wallet_test_encrypted )
{
    printf("Testing ecrypted wallet - good password\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "tests/armory_pv6Mmjdh_encrypt.wallet" ), "armory" );

    printf("found %d keys\n", (int)keys.size());
    BOOST_REQUIRE( (keys.size() == 104) );

    std::vector<std::string> addrs;
    for (auto &key : keys) {
        bts::blockchain::pts_address a_c( key.get_public_key(), true );
        std::string addr = a_c;
        addrs.push_back( addr );
    }
    std::sort( addrs.begin(), addrs.end() );

    BOOST_REQUIRE( addrs[99] == "PvgkS8QwqEVGWVzFfnAaXUCWRX76RU3CfV" );
    BOOST_REQUIRE( addrs[96] == "PutWpxxTvUxNHXjDFmBf5stf6ATenQqzQ2" );
    BOOST_REQUIRE( addrs[77] == "PqT3Ub3jWg8fjvjjUo4xMAhB5MWKJB1i32" );
    BOOST_REQUIRE( addrs[34] == "PfxMkC5gUqGevh3v3sFHgCvu9pxYQUPYna" );
}

BOOST_AUTO_TEST_CASE( import_armory_wallet_test_encrypted_wrong_pass )
{
    printf("Testing ecrypted wallet - bad password\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "tests/armory_pv6Mmjdh_encrypt.wallet" ), "notthepassword" );

    printf("found %d keys\n", (int)keys.size());
    BOOST_REQUIRE( (keys.size() == 0) );
}

