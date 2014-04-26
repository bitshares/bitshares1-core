#define BOOST_TEST_MODULE BlockchainTests
#include <boost/test/unit_test.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/block_miner.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/filesystem.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>
using namespace bts::wallet;
using namespace bts::blockchain;

trx_block generate_genesis_block( const std::vector<address>& addr )
{
    trx_block genesis;
    genesis.version           = 0;
    genesis.block_num         = 0;
    genesis.timestamp         = fc::time_point::now();
    genesis.next_fee          = block_header::min_fee();
    genesis.total_shares      = 0;

    signed_transaction dtrx;
    dtrx.vote = 0;
    // create initial delegates
    for( uint32_t i = 0; i < 100; ++i )
    {
       auto name     = "delegate-"+fc::to_string( int64_t(i+1) );
       auto key_hash = fc::sha256::hash( name.c_str(), name.size() );
       auto key      = fc::ecc::private_key::regenerate(key_hash);
       dtrx.outputs.push_back( trx_output( claim_name_output( name, std::string(), i+1, key.get_public_key() ), asset() ) );
    }
    genesis.trxs.push_back( dtrx );

    // generate an initial genesis block that evenly allocates votes among all
    // delegates.
    for( uint32_t i = 0; i < 100; ++i )
    {
       signed_transaction trx;
       trx.vote = i + 1;
       for( uint32_t o = 0; o < 5; ++o )
       {
          uint64_t amnt = 200000;
          trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
          genesis.total_shares += amnt;
       }
       genesis.trxs.push_back( trx );
    }

    genesis.trx_mroot = genesis.calculate_merkle_root(signed_transactions());

    return genesis;
}

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
   try {
       fc::temp_directory dir;
       wallet             wall;
       wall.create_internal( dir.path() / "wallet.dat", "password", "password", true );

       for( uint32_t i = 0; i < 100; ++i )
       {
          auto name     = "delegate-"+fc::to_string( int64_t(i+1) );
          auto key_hash = fc::sha256::hash( name.c_str(), name.size() );
          auto key      = fc::ecc::private_key::regenerate(key_hash);
          wall.import_delegate( i+1, key );
       }

       fc::ecc::private_key auth = fc::ecc::private_key::generate();

       std::vector<address> addrs;
       addrs.reserve(80);
       for( uint32_t i = 0; i < 80; ++i )
       {
          addrs.push_back( wall.new_receive_address() );
       }

       chain_database     db;
       db.set_trustee( auth.get_public_key() );
       auto sim_validator = std::make_shared<sim_pow_validator>( fc::time_point::now() );
       db.set_pow_validator( sim_validator );
       db.open( dir.path() / "chain" );
       auto genblk = generate_genesis_block( addrs );
       genblk.sign(auth);
       db.push_block( genblk );

       wall.scan_chain( db );
       wall.dump_unspent_outputs();
       //db.dump_delegates();

       for( uint32_t i = 0; i < 1000; ++i )
       {
          std::vector<signed_transaction> trxs;
          for( uint32_t i = 0; i < 5; ++i )
          {
             auto trx = wall.transfer( asset( double( rand() % 1000 ) ), addrs[ rand()%addrs.size() ] );
             trxs.push_back( trx );
          }
          sim_validator->skip_time( fc::seconds(60*5) );
          auto next_block = wall.generate_next_block( db, trxs );
          ilog( "block: ${b}", ("b", next_block ) );
          sim_validator->skip_time( fc::seconds(30) );
          next_block.sign( auth );
          db.push_block( next_block );
          auto head_id = db.head_block_id();

          if( i % 10 == 0 )
          {
              wall.scan_chain( db );
              wall.dump_unspent_outputs();
          }
          //db.dump_delegates();
       }
   }
   catch ( const fc::exception& e )
   {
      std::cerr<<e.to_detail_string()<<"\n";
      elog( "${e}", ( "e", e.to_detail_string() ) );
      throw;
   }
} // blockchain_simple_chain


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
