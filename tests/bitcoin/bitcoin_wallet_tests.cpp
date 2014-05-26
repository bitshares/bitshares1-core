#define BOOST_TEST_MODULE BitcoinWalletTest
#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/assert.hpp>
#include <fc/filesystem.hpp>

#include <bts/bitcoin/bitcoin.hpp>
#include <bts/blockchain/pts_address.hpp>

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
