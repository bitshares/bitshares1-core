#define BOOST_TEST_MODULE ElectrumWalletTest
#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/assert.hpp>
#include <fc/filesystem.hpp>

#include <bts/bitcoin/electrum.hpp>
#include <bts/blockchain/pts_address.hpp>

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
