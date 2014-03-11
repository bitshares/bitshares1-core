#define BOOST_TEST_MODULE BlockchainTests
#include <boost/test/unit_test.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/filesystem.hpp>

using namespace bts::wallet;

/**
 *  The purpose of this test is to make sure that the network will
 *  not stall even with random transactions executing as quickly
 *  as possible.  
 *
 *  This test will generate 1000 wallets, each starting with a random
 *  balance.   Then it will randomly make 10 transactions from 10
 *  wallets to 10 other wallets.  Generate a block and push it
 *  on to the blockchain.   
 *
 *  It will do this for one years worth of blocks. 
 *
 */
BOOST_AUTO_TEST_CASE( blockchain_endurence )
{
    
}

/**
 *  This test creates a single wallet and a genesis block
 *  initialized with 1000 starting addresses.  It will then
 *  generate one random transaction per block and grow
 *  the blockchain as quickly as possible.  
 */
BOOST_AUTO_TEST_CASE( blockchain_simple_chain )
{
   fc::temp_directory dir;
   wallet             wall;
   wall.create( dir.path() / "wallet.dat", "pass", "pass", true );

   
}

/**
 *  This test case will generate two wallets, generate
 *  a years worth of transactions from one wallet and 
 *  then verify that the inactive outputs are brought
 *  forward and pay a 5% fee.
 */
BOOST_AUTO_TEST_CASE( blockchain_inactivity_fee )
{

}

/**
 *  This test is designed to ensure that blocks that attempt
 *  a doublespend are rejected.
 */
BOOST_AUTO_TEST_CASE( blockchain_doublespend )
{

}

/**
 *  This test case verifies that the head block can be replaced by
 *  a better block.  A better block is one that contains more votes.
 */
BOOST_AUTO_TEST_CASE( blockchain_replace_head_block )
{

}
