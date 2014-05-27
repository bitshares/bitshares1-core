#define BOOST_TEST_MODULE BitcoinWalletImportTests

#include <algorithm>
#include <array>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <bts/bitcoin/armory.hpp>
#include <bts/bitcoin/bitcoin.hpp>
#include <bts/bitcoin/electrum.hpp>
#include <bts/bitcoin/multibit.hpp>

#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>

using namespace bts::bitcoin;
using namespace bts::blockchain;

const std::array<std::array<std::string, 4>, 6> wallets = 
{{
    {{"bitcoin", "bitcoin.dat", "", "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt"}},
    {{"bitcoin", "bitcoin_encrypted.dat", "1234password", "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt"}},
    {{"electrum", "electrum.dat", "", "PYDyNCfUvvv3y5JgY5v1rvRSRHPm3nCwDU"}},
    {{"electrum", "electrum_encrypted.dat", "1234password", "PYDyNCfUvvv3y5JgY5v1rvRSRHPm3nCwDU"}},
    {{"multibit", "multibit.wallet", "", "PYkCymYWjVRQb5pCCCPChkDSNVbtWQdFP7"}},
    {{"multibit", "multibit_encrypted.wallet", "1234password", "PYkCymYWjVRQb5pCCCPChkDSNVbtWQdFP7"}}
}};

// TODO: check more than 1 address

const std::vector<private_key_type> import_wallet( const std::string& type, const fc::path& filename, const std::string& password)
{
    if( type == "bitcoin" )
        return import_bitcoin_wallet( filename, password );
    else if( type == "armory" )
        return import_armory_wallet( filename, password );
    else if( type == "electrum" )
        return import_electrum_wallet( filename, password );
    else if( type == "multibit" )
        return import_multibit_wallet( filename, password );

    return std::vector<private_key_type>();
}

BOOST_AUTO_TEST_CASE( wallet_import_tests )
{
    for( const auto& wallet : wallets )
    {
        printf( "\n" );
        try
        {
            const auto& type     = wallet[0];
            const auto& filename = wallet[1];
            const auto& password = wallet[2];
            const auto& addr     = wallet[3];

            FC_ASSERT( fc::exists( filename ), "Wallet file \"${f}\" not found", ("f", filename));

            printf( "Testing %s wallet \"%s\" with password \"%s\"\n", type.c_str(), filename.c_str(), password.c_str() );

            if( !password.empty() )
            {
                try
                {
                    import_wallet( type, fc::path( filename ), "" );
                    // TODO
                }
                catch( ... )
                {
                }
            }

            auto keys = import_wallet( type, fc::path( filename ), password );
            BOOST_REQUIRE( keys.size() >= 1 );
            printf( "Found %lu keys\n", keys.size() );

            std::vector<pts_address> addrs;
            for( const auto& key : keys )
            {
                addrs.push_back( pts_address( key.get_public_key() ) );
            }

            printf( "Checking for address %s\n", addr.c_str() );
            BOOST_REQUIRE( std::find( addrs.begin(), addrs.end(), pts_address( addr ) ) != addrs.end() );
            printf( "PASSED\n" );
        }
        catch( const fc::exception& e )
        {
            printf( "%s\n", e.to_detail_string().c_str() );
            printf( "FAILED\n" );
        }
        catch( ... )
        {
            printf( "FAILED\n" );
        }
    }
}

#if 0
BOOST_AUTO_TEST_CASE( import_bitcoin_wallet_test_unencrypted )
{
    printf("Testing unencrypted PTS wallet\n");
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "pts_wallet.dat" ), "" );
    printf("found %d keys\n", (int)keys.size());
	BOOST_REQUIRE( (keys.size() >= 1) );

	std::vector<std::string> addrs;
	for (auto &key : keys) {
        bts::blockchain::pts_address a_c( key.get_public_key() );
		std::string addr = a_c;
		addrs.push_back( addr );
	}
	std::sort( addrs.begin(), addrs.end() );

	//BOOST_REQUIRE( addrs[0] == "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt" );
	//BOOST_REQUIRE( addrs[1] == "PgbAZBkrd9AKDt669wB6GsLRiprj4FbPDL" );
	//BOOST_REQUIRE( addrs[2] == "PhAVnkFGMYao7gtTotFg4WiT8gHsh9VxEg" );
	//BOOST_REQUIRE( addrs[3] == "Pm1eTeEyKU8LmaqdUrUBDbhUsrWDg97gcy" );

	BOOST_REQUIRE( std::find( addrs.begin(), addrs.end(), "Pb8Sxdme4pLzQnir5VT9NMxutjPybiaMNc" ) != addrs.end() );
}

BOOST_AUTO_TEST_CASE( import_bitcoin_wallet_test_encrypted )
{
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "wallet-encrypted.dat" ), "1234password" );

	BOOST_REQUIRE( (keys.size() == 104) );

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
	std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_bitcoin_wallet( fc::path ( "wallet-encrypted.dat" ), "whatisit" );

	BOOST_REQUIRE( (keys.size() == 0) );
}

BOOST_AUTO_TEST_CASE( import_electrum_wallet_test_unencrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "electrum-wallet-unencrypted.dat" ), "" );
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
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "electrum-wallet-encrypted.dat" ), "1234password" );

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
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_electrum_wallet( fc::path ( "electrum-wallet-encrypted.dat" ), "whatisit" );

   BOOST_REQUIRE( (keys.size() == 0) );
}

BOOST_AUTO_TEST_CASE( import_multibit_wallet_test_unencrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "multibit-unencrypted.wallet" ), "" );

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
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "multibit-encrypted.wallet" ), "1234password");

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
   std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_multibit_wallet( fc::path ( "multibit-encrypted.wallet" ), "whatisit" );

   BOOST_REQUIRE( (keys.size() == 0) );
}
#endif
#if 0
BOOST_AUTO_TEST_CASE( import_armory_wallet_test_unencrypted )
{
    printf("Testing unencrypted wallet\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "armory_pv6Mmjdh_decrypt.wallet" ), "" );
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
    printf("Testing encrypted wallet - good password\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "armory_pv6Mmjdh_encrypt.wallet" ), "armory" );

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
    printf("Testing encrypted wallet - bad password\n");
    std::vector<fc::ecc::private_key> keys = bts::bitcoin::import_armory_wallet( fc::path ( "armory_pv6Mmjdh_encrypt.wallet" ), "notthepassword" );

    printf("found %d keys\n", (int)keys.size());
    BOOST_REQUIRE( (keys.size() == 0) );
}
#endif
