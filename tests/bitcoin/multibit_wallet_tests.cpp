#define BOOST_TEST_MODULE MultibitWalletTest
#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/assert.hpp>
#include <fc/filesystem.hpp>

#include <bts/bitcoin/multibit.hpp>
#include <bts/blockchain/pts_address.hpp>

BOOST_AUTO_TEST_CASE( import_multibit_wallet_test_unencrypted )
{
   std::vector<fc::ecc::private_key> keys = bts::import_multibit_wallet( fc::path ( "tests/multibit-unencrypted.wallet" ), "" );

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
   std::vector<fc::ecc::private_key> keys = bts::import_multibit_wallet( fc::path ( "tests/multibit-encrypted.wallet" ), "1234password");

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
   std::vector<fc::ecc::private_key> keys = bts::import_multibit_wallet( fc::path ( "tests/multibit-encrypted.wallet" ), "whatisit" );

   BOOST_REQUIRE( (keys.size() == 0) );
}
