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

#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>

using namespace bts::bitcoin;
using namespace bts::blockchain;

const std::array<std::array<std::string, 3>, 8> wallets =
{{
    {{"bitcoin", "bitcoin.dat", ""}},
    {{"bitcoin", "bitcoin_encrypted.dat", "1234password"}},
    {{"armory", "armory.wallet", ""}},
    {{"armory", "armory_encrypted.wallet", "armory"}},
    {{"electrum", "electrum.dat", ""}},
    {{"electrum", "electrum_encrypted.dat", "1234password"}},
    {{"multibit", "multibit.wallet", ""}},
    {{"multibit", "multibit_encrypted.wallet", "1234password"}}
}};

const std::array<std::string, 4> bitcoin_addresses =
{{
    "PekgH7fspAH4SYoxn8JU38xYtEhNkKfDpt",
    "PgbAZBkrd9AKDt669wB6GsLRiprj4FbPDL",
    "PhAVnkFGMYao7gtTotFg4WiT8gHsh9VxEg",
    "Pm1eTeEyKU8LmaqdUrUBDbhUsrWDg97gcy"
}};

const std::array<std::string, 4> armory_addresses =
{{
    "PvgkS8QwqEVGWVzFfnAaXUCWRX76RU3CfV",
    "PutWpxxTvUxNHXjDFmBf5stf6ATenQqzQ2",
    "PqT3Ub3jWg8fjvjjUo4xMAhB5MWKJB1i32",
    "PfxMkC5gUqGevh3v3sFHgCvu9pxYQUPYna"
}};

const std::array<std::string, 8> electrum_addresses =
{{
    "PYDyNCfUvvv3y5JgY5v1rvRSRHPm3nCwDU",
    "PdY9WLrtT7YPpNErwgUJDArFbkrAn8ZWff",
    "PdrswsSjVWvizhih1SzacCagcqh1jbReA1",
    "PhBkZJKkEeYcZ8WQiZvvSFBS2mkYFUaZFZ",
    "PiFYvWLQTquGpSF3hn5TS5WZcS4x6cY6qt",
    "PqxrSuWHBXiK4p7K85MdESDgjdrfcLNcm2",
    "Psmgnz8pqYaYMj5iLvP3cPCrcu7PDPWHmR",
    "PvfazYh28KP6gEnwyCZqLFBTj7cN3DwBY4"
}};

const std::array<std::string, 4> multibit_addresses =
{{
    "PYkCymYWjVRQb5pCCCPChkDSNVbtWQdFP7",
    "PooEyUSnQRbeKN149gymSD7Ten6xWgyNXU",
    "PuNg5hRE8XH1PbneeUCZdUjtAvkRMbvDTu",
    "PvthDffkUBHGTJMrxpxP1BByy4qbWH5yJ1"
}};

const std::vector<private_key_type> import_wallet( const std::string& type, const fc::path& filename, const std::string& password,
                                                   std::vector<pts_address>& addrs )
{
    if( type == "bitcoin" )
    {
        for( const auto& addr : bitcoin_addresses )
            addrs.push_back( pts_address( addr ) );

        return import_bitcoin_wallet( filename, password );
    }
#if 0
    else if( type == "armory" )
    {
        for( const auto& addr : armory_addresses )
            addrs.push_back( pts_address( addr ) );

        return import_armory_wallet( filename, password );
    }
#endif
    else if( type == "electrum" )
    {
        for( const auto& addr : electrum_addresses )
            addrs.push_back( pts_address( addr ) );

        return import_electrum_wallet( filename, password );
    }
#if 0
    else if( type == "multibit" )
    {
        for( const auto& addr : multibit_addresses )
            addrs.push_back( pts_address( addr ) );

        return import_multibit_wallet( filename, password );
    }
#endif

    return std::vector<private_key_type>();
}

const std::vector<private_key_type> import_wallet( const std::string& type, const fc::path& filename, const std::string& password )
{
    auto addrs = std::vector<pts_address>();
    return import_wallet( type, filename, password, addrs );
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

            FC_ASSERT( fc::exists( filename ), "Wallet file \"${f}\" not found", ("f", filename));

            printf( "Testing %s wallet \"%s\" with password \"%s\"\n", type.c_str(), filename.c_str(), password.c_str() );

            auto keys = std::vector<fc::ecc::private_key>();
            if( !password.empty() )
            {
                try
                {
                    keys = import_wallet( type, fc::path( filename ), "" );
                }
                catch( ... )
                {
                }
                BOOST_REQUIRE( keys.size() <= 0 );
            }

            auto test_addrs = std::vector<pts_address>();
            keys = import_wallet( type, fc::path( filename ), password, test_addrs );
            BOOST_REQUIRE( keys.size() >= 1 );
            printf( "Found %lu keys\n", keys.size() );

            std::vector<pts_address> imported_addrs;
            for( const auto& key : keys )
                imported_addrs.push_back( pts_address( key.get_public_key() ) );

            for( const auto& test_addr : test_addrs )
            {
                printf( "Checking for address %s\n", std::string( test_addr ).c_str() );
                BOOST_REQUIRE( std::find( imported_addrs.begin(), imported_addrs.end(), test_addr ) != imported_addrs.end() );
            }
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
