#define BOOST_TEST_MODULE BitcoinWalletTest
#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/assert.hpp>
#include <fc/filesystem.hpp>

#include <bts/bitcoin/armory.hpp>
#include <bts/bitcoin/bitcoin.hpp>
#include <bts/bitcoin/electrum.hpp>
#include <bts/bitcoin/multibit.hpp>
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

BOOST_AUTO_TEST_CASE( import_bitcoin_wallet_test_unencrypted )
{
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "tests/wallet-unencrypted.dat" ), "" );
	BOOST_REQUIRE( (keys.size() == 4) );

	std::vector<std::string> addrs;
	for (auto &key : keys) {
        bts::blockchain::pts_address a_c( key.get_public_key(), true );
		std::string addr = a_c;
		addrs.push_back( addr );
	}
	std::sort( addrs.begin(), addrs.end() );

	BOOST_REQUIRE( addrs[0] == "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt" );
	BOOST_REQUIRE( addrs[1] == "PgbAZBkrd9AKDt669wB6GsLRiprj4FbPDL" );
	BOOST_REQUIRE( addrs[2] == "PhAVnkFGMYao7gtTotFg4WiT8gHsh9VxEg" );
	BOOST_REQUIRE( addrs[3] == "Pm1eTeEyKU8LmaqdUrUBDbhUsrWDg97gcy" );
}

BOOST_AUTO_TEST_CASE( import_bitcoin_wallet_test_encrypted )
{
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "tests/wallet-encrypted.dat" ), "1234password" );

	BOOST_REQUIRE( (keys.size() == 4) );

	std::vector<std::string> addrs;
	for (auto &key : keys) {
        bts::blockchain::pts_address a_c( key.get_public_key(), true );
		std::string addr = a_c;
		addrs.push_back( addr );
	}
	std::sort( addrs.begin(), addrs.end() );

	BOOST_REQUIRE( addrs[0] == "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt" );
	BOOST_REQUIRE( addrs[1] == "PgbAZBkrd9AKDt669wB6GsLRiprj4FbPDL" );
	BOOST_REQUIRE( addrs[2] == "PhAVnkFGMYao7gtTotFg4WiT8gHsh9VxEg" );
	BOOST_REQUIRE( addrs[3] == "Pm1eTeEyKU8LmaqdUrUBDbhUsrWDg97gcy" );
}

BOOST_AUTO_TEST_CASE( import_bitcoin_wallet_test_encrypted_wrong_pass )
{
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "tests/wallet-encrypted.dat" ), "whatisit" );

	BOOST_REQUIRE( (keys.size() == 0) );
}

BOOST_AUTO_TEST_CASE( import_electrum_wallet_test_unencrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "tests/electrum-wallet-unencrypted.dat" ), "" );
   BOOST_REQUIRE( (keys.size() == 8) );

   std::vector<std::string> addrs;
   for (auto &key : keys) {
      bts::blockchain::pts_address a_c( key.get_public_key(), true );
      std::string addr = a_c;
      addrs.push_back( addr );
   }
   std::sort( addrs.begin(), addrs.end() );

   BOOST_REQUIRE( addrs[0] == "PYDyNCfUvvv3y5JgY5v1rvRSRHPm3nCwDU" );
   BOOST_REQUIRE( addrs[1] == "PdY9WLrtT7YPpNErwgUJDArFbkrAn8ZWff" );
   BOOST_REQUIRE( addrs[2] == "PdrswsSjVWvizhih1SzacCagcqh1jbReA1" );
   BOOST_REQUIRE( addrs[3] == "PhBkZJKkEeYcZ8WQiZvvSFBS2mkYFUaZFZ" );
   BOOST_REQUIRE( addrs[4] == "PiFYvWLQTquGpSF3hn5TS5WZcS4x6cY6qt" );
   BOOST_REQUIRE( addrs[5] == "PqxrSuWHBXiK4p7K85MdESDgjdrfcLNcm2" );
   BOOST_REQUIRE( addrs[6] == "Psmgnz8pqYaYMj5iLvP3cPCrcu7PDPWHmR" );
   BOOST_REQUIRE( addrs[7] == "PvfazYh28KP6gEnwyCZqLFBTj7cN3DwBY4" );
}

BOOST_AUTO_TEST_CASE( import_electrum_wallet_test_encrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "tests/electrum-wallet-encrypted.dat" ), "1234password" );

   BOOST_REQUIRE( (keys.size() == 8) );

   std::vector<std::string> addrs;
   for (auto &key : keys) {
      bts::blockchain::pts_address a_c( key.get_public_key(), true );
      std::string addr = a_c;
      addrs.push_back( addr );
   }
   std::sort( addrs.begin(), addrs.end() );

   BOOST_REQUIRE( addrs[0] == "PYDyNCfUvvv3y5JgY5v1rvRSRHPm3nCwDU" );
   BOOST_REQUIRE( addrs[1] == "PdY9WLrtT7YPpNErwgUJDArFbkrAn8ZWff" );
   BOOST_REQUIRE( addrs[2] == "PdrswsSjVWvizhih1SzacCagcqh1jbReA1" );
   BOOST_REQUIRE( addrs[3] == "PhBkZJKkEeYcZ8WQiZvvSFBS2mkYFUaZFZ" );
   BOOST_REQUIRE( addrs[4] == "PiFYvWLQTquGpSF3hn5TS5WZcS4x6cY6qt" );
   BOOST_REQUIRE( addrs[5] == "PqxrSuWHBXiK4p7K85MdESDgjdrfcLNcm2" );
   BOOST_REQUIRE( addrs[6] == "Psmgnz8pqYaYMj5iLvP3cPCrcu7PDPWHmR" );
   BOOST_REQUIRE( addrs[7] == "PvfazYh28KP6gEnwyCZqLFBTj7cN3DwBY4" );
}

BOOST_AUTO_TEST_CASE( import_electrum_wallet_test_encrypted_wrong_pass )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "tests/electrum-wallet-encrypted.dat" ), "whatisit" );

   BOOST_REQUIRE( (keys.size() == 0) );
}

BOOST_AUTO_TEST_CASE( import_multibit_wallet_test_unencrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "tests/multibit-unencrypted.wallet" ), "" );

   BOOST_REQUIRE( (keys.size() == 4) );

   std::vector<std::string> addrs;
   for (auto &key : keys) {
      bts::blockchain::pts_address a_c( key.get_public_key(), true );
      std::string addr = a_c;
      addrs.push_back( addr );
   }
   std::sort( addrs.begin(), addrs.end() );

   BOOST_REQUIRE( addrs[0] == "PYkCymYWjVRQb5pCCCPChkDSNVbtWQdFP7" );
   BOOST_REQUIRE( addrs[1] == "PooEyUSnQRbeKN149gymSD7Ten6xWgyNXU" );
   BOOST_REQUIRE( addrs[2] == "PuNg5hRE8XH1PbneeUCZdUjtAvkRMbvDTu" );
   BOOST_REQUIRE( addrs[3] == "PvthDffkUBHGTJMrxpxP1BByy4qbWH5yJ1" );
}

BOOST_AUTO_TEST_CASE( import_multibit_wallet_test_encrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "tests/multibit-encrypted.wallet" ), "1234password");

   BOOST_REQUIRE( (keys.size() == 4) );

   std::vector<std::string> addrs;
   for (auto &key : keys) {
      bts::blockchain::pts_address a_c( key.get_public_key(), true );
      std::string addr = a_c;
      addrs.push_back( addr );
   }
   std::sort( addrs.begin(), addrs.end() );

   BOOST_REQUIRE( addrs[0] == "PYkCymYWjVRQb5pCCCPChkDSNVbtWQdFP7" );
   BOOST_REQUIRE( addrs[1] == "PooEyUSnQRbeKN149gymSD7Ten6xWgyNXU" );
   BOOST_REQUIRE( addrs[2] == "PuNg5hRE8XH1PbneeUCZdUjtAvkRMbvDTu" );
   BOOST_REQUIRE( addrs[3] == "PvthDffkUBHGTJMrxpxP1BByy4qbWH5yJ1" );
}

BOOST_AUTO_TEST_CASE( import_multibit_wallet_test_encrypted_wrong_pass )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "tests/multibit-encrypted.wallet" ), "whatisit" );

   BOOST_REQUIRE( (keys.size() == 0) );
}
