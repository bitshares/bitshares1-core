#define BOOST_TEST_MODULE BlockchainTests
#include <boost/test/unit_test.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/block_miner.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/block.hpp>
#include <fc/filesystem.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>
#include <bts/net/chain_server.hpp>

#include <iostream>
#include <iomanip>
using namespace bts::wallet;
using namespace bts::blockchain;

   void list_delegates( chain_database& db )
   {
        auto delegates = db.get_delegates( );

        std::cerr<<"Delegate Ranking\n";
        std::cerr<<std::setw(6)<<"Rank "<<"  "
                 <<std::setw(6)<<"ID"<<"  "
                 <<std::setw(18)<<"NAME"<<"  "
                 <<std::setw(18)<<"VOTES FOR"<<"  "
                 <<std::setw(18)<<"VOTES AGAINST"<<"  "
                 <<"    PERCENT\n";
        std::cerr<<"==========================================================================================\n";
        for( uint32_t i = 0; i < delegates.size(); ++i )
        {
           std::cerr << std::setw(6)  << i               << "  "
                     << std::setw(6)  << delegates[i].delegate_id   << "  "
                     << std::setw(18) << delegates[i].name          << "  "
                     << std::setw(18) << delegates[i].votes_for     << "  "
                     << std::setw(18) << delegates[i].votes_against << "  "
                     << std::setw(18) << double((delegates[i].total_votes()*10000)/BTS_BLOCKCHAIN_BIP)/100  << "%   |\n";
           ++i;
        }

      // client()->get_chain()->dump_delegates( count );
   }

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
       dtrx.outputs.push_back( trx_output( claim_name_output( name, std::string(), i+1, key.get_public_key(), key.get_public_key() ), asset() ) );
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
 *  Test the following:
 *    1) Register a name
 *    2) Transfer a name
 *    3) Update a name
 *    4) Prevent Duplicate Registration
 *    5) Validate data format is JSON
 */
BOOST_AUTO_TEST_CASE( blockchain_name_reservation )
{
   try {
       fc::temp_directory dir;
       wallet             wall;
       wallet             name_wallet;
       name_wallet.create_internal( dir.path() / "name_wallet.dat", "password2", "password2", true );
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
       for( uint32_t i = 0; i < 79; ++i )
       {
          addrs.push_back( wall.new_receive_address() );
       }
       addrs.push_back( name_wallet.new_receive_address() );

       chain_database     db;
       db.set_trustee( auth.get_public_key() );
       auto sim_validator = std::make_shared<sim_pow_validator>( fc::time_point::now() );
       db.set_pow_validator( sim_validator );
       db.open( dir.path() / "chain" );
       auto genblk = generate_genesis_block( addrs );
       genblk.sign(auth);
       db.push_block( genblk );

       wall.scan_chain( db );
       name_wallet.scan_chain( db );
  //     wall.dump_unspent_outputs();

       std::vector<signed_transaction> trxs;
       auto trx = name_wallet.reserve_name( "dan", fc::variant("checkdata") );
       trxs.push_back( trx );

       sim_validator->skip_time( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
       auto next_block = wall.generate_next_block( db, trxs );
       ilog( "block: ${b}", ("b", next_block ) );
       sim_validator->skip_time( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
       next_block.sign( auth );
       db.push_block( next_block );

       wall.scan_chain( db );
       name_wallet.scan_chain( db );
       name_wallet.dump_unspent_outputs();

       // this should throw an exception because the name dan is in use by name_wallet
       //  testing the wallet generation is not good enough, we must also assume the
       //  wallet produced a valid transaction and verify that the blockchain
       //  rejected it.
       bool caught_duplicate_name = false;
       try {
          trx = wall.reserve_name( "dan", fc::variant("checkdata2") );
       } catch ( const fc::exception& e )
       {
          wlog( "expected ${e}", ("e",e.to_detail_string() ) );
          caught_duplicate_name = true;
       }
       FC_ASSERT( caught_duplicate_name );

       // this should produce a valid transaction that updates the name
       trx = name_wallet.reserve_name( "dan", fc::variant("checkdata3") );

       // attempt to reserve it again...
       {
          try {
            db.evaluate_transaction( trx );
          }
          catch ( const fc::exception& e )
          {
             elog( "update failed: ${e}", ("e",e.to_detail_string() ) );
             throw;
          }

          trxs.back() = trx;

          sim_validator->skip_time( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
          auto next_block = wall.generate_next_block( db, trxs );
          ilog( "block: ${b}", ("b", next_block ) );
          sim_validator->skip_time( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
          next_block.sign( auth );
          db.push_block( next_block );

          wall.scan_chain( db );
       }
       // attempt to update name dan
       {

       }

       name_wallet.scan_chain(db);
       name_wallet.dump_unspent_outputs();


       //wall.dump_unspent_outputs();
   }
   catch ( const fc::exception& e )
   {
      std::cerr<<e.to_detail_string()<<"\n";
      elog( "${e}", ( "e", e.to_detail_string() ) );
      throw;
   }
} // blockchain_simple_chain

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
             auto trx = wall.send_to_address( asset( double( rand() % 1000 ) ), addrs[ rand()%addrs.size() ] );
             trxs.push_back( trx );
          }
          sim_validator->skip_time( fc::seconds(60*5) );
          auto next_block = wall.generate_next_block( db, trxs );
          ilog( "block: ${b}", ("b", next_block ) );
          sim_validator->skip_time( fc::seconds(30) );
          next_block.sign( auth );
          db.push_block( next_block );

          if( i % 10 == 0 )
          {
              wall.scan_chain( db );
              wall.dump_unspent_outputs();
          }

          list_delegates( db );
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

/**
 * this test case is intended to replicate one of the bts_xt_client_test
 * cases that was failing, but without the processes and networking
 * complicating things.
 * Here we generate a number of wallet and send as many transactions
 * from each wallet out to all the other wallets, until we can't
 * find any more unspent outputs to pull from.  Then we wait for the
 * next block and do the same thing.  We should be able to make as
 * many transactions as before, since each transaction in the
 * previous round should have generated a change address we can
 * use as an input to this round.
 */
BOOST_AUTO_TEST_CASE( blockchain_test_change_address_processing )
{
  boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);

  const uint32_t wallet_count = 50;
  try
  {
    fc::path base_dir = fc::temp_directory_path() / "blockchain_tests";
    fc::remove_all(base_dir);

    bts::net::genesis_block_config genesis_block_config;
    fc::ecc::private_key trustee_key = fc::ecc::private_key::generate();

    struct client_info
    {
      fc::ecc::private_key initial_key;
      uint64_t initial_balance;
      wallet_ptr wallet;
      bts::blockchain::address receive_address;
    };

    std::vector<client_info> clients;

    for (uint32_t i = 0; i < wallet_count; ++i)
    {
      client_info info;
      info.initial_balance = 100000000;
      info.initial_key = fc::ecc::private_key::generate();

      genesis_block_config.balances.push_back(std::make_pair(bts::blockchain::pts_address(info.initial_key.get_public_key()),
                                                             info.initial_balance / 100000000));

      std::ostringstream wallet_dir_name;
      wallet_dir_name << "wallet_" << i;
      fc::path wallet_path = base_dir / wallet_dir_name.str();
      fc::create_directories(wallet_path);
      info.wallet = std::make_shared<bts::wallet::wallet>();
      info.wallet->create_internal(wallet_path / "wallet.dat", "password", "password", false);
      info.receive_address = info.wallet->new_receive_address("test_rx_address");
      clients.push_back(info);
    }

    fc::path genesis_json = base_dir / "genesis.json";
    fc::json::save_to_file(genesis_block_config, genesis_json, true);
    bts::blockchain::trx_block genesis_block = create_test_genesis_block(genesis_json);

    chain_database blockchain;
    blockchain.set_trustee( trustee_key.get_public_key() );
    blockchain.open( base_dir / "chain" );
    genesis_block.sign(trustee_key);
    blockchain.push_block(genesis_block);

    for (uint32_t i = 0; i < wallet_count; ++i)
    {
      BOOST_CHECK(clients[i].wallet->get_balance(0).amount == 0);
      clients[i].wallet->import_key(clients[i].initial_key, "initial_key");
      clients[i].wallet->scan_chain(blockchain);
      BOOST_CHECK(clients[i].wallet->get_balance(0).amount == clients[i].initial_balance);
    }

    for (int loop_count = 0; loop_count < 10; ++loop_count)
    {
      std::vector<signed_transaction> transactions;
      uint32_t next_recipient = 1;
      for (uint32_t client_index = 0; client_index < wallet_count; ++client_index)
      {
        clients[client_index].wallet->dump_unspent_outputs();
        BOOST_TEST_MESSAGE("initial balance " << clients[client_index].wallet->get_balance(0).amount);
        int transfer_count = 0;
        for (;;) // send money for as long as we think we have money to send
        {
          try
          {
            asset starting_balance = clients[client_index].wallet->get_balance(0);
            signed_transaction transaction = clients[client_index].wallet->send_to_address(asset(10),
                                                                                           clients[next_recipient].receive_address);
            transactions.push_back(transaction);
            asset ending_balance = clients[client_index].wallet->get_balance(0);
            BOOST_CHECK(ending_balance <= starting_balance - 10);
            //BOOST_CHECK(ending_balance >= starting_balance - 10 - 500 /* 500 is way larger than max expected tx fee */);
            next_recipient = (next_recipient + 1) % wallet_count;
            if (next_recipient == client_index)
              next_recipient = (next_recipient + 1) % wallet_count;
            ++transfer_count;
          }
          catch (const fc::exception&)
          {
            ilog("Only able to send ${count} transactions from client ${client}", ("count", transfer_count)("client", client_index));
            BOOST_TEST_MESSAGE("ending balance " << clients[client_index].wallet->get_balance(0).amount);
            break;
          }
        }
      } // each wallet

      trx_block next_block = clients[0].wallet->generate_next_block(blockchain, transactions);
      next_block.sign(trustee_key);
      ilog( "generated block: ${b}", ("b", next_block ));
      blockchain.push_block(next_block);
      for (uint32_t client_index = 0; client_index < wallet_count; ++client_index)
        clients[client_index].wallet->scan_chain(blockchain, blockchain.get_head_block().block_num);
    } // each iteration of the test
  }// try
  catch (const fc::exception& e)
  {
    std::cerr<<e.to_detail_string()<<"\n";
    elog( "${e}", ( "e", e.to_detail_string() ) );
    throw;
  }
}
#if 0

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

}
#endif
