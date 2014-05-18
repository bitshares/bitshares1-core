#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>

using namespace bts::blockchain;
using namespace bts::wallet;

const char* test_keys = R"([
  "dce167e01dfd6904015a8106e0e1470110ef2d5b0b18ba7a83cb8204e25c6b5f",
  "7fee1dc3110ba4abe134822c257c9db5beadbe557763cc54e3dc59b699978a50",
  "4e42e82970d3307d26634572cddbf8424c10cee1c8c7fcebdd9417942a08a1bd",
  "e44baa4f693f1bd71ba8f6b8509c56dd41206e2cff07efcf7b42fc79464a1c59",
  "18fd5207b1a0d72465b8f3ed06b44232a366a76f559c5da3aec9f306e179278f",
  "6dc61837b51d0bb80143c1a7ba5c957f122faa96e0a6ff76ba25f9ff42ef69fd",
  "45b0a66b5016618700b4d4fbfa85efd1b0724e8ae95b09a6c9ec850a05254f48",
  "602751d179b75da5432c64a3360a7309609636056c31deae37b8441230c968eb",
  "8ced7ac956e1755e87c21b1b265979c848c79b3bea3b855bb5b819d3baa72ed2",
  "90ef5e50773c90368597e46eaf1b563f76f879aa8969c2e7a2198847f93324c4"
])"; 

BOOST_AUTO_TEST_CASE( block_signing )
{
   try {
      fc::ecc::private_key signer = fc::ecc::private_key::generate();
      auto pub_key = signer.get_public_key();
      ilog( "pub_key: ${pub}", ("pub",pub_key) );
      full_block blk;

      signed_block_header& header = blk;
      header.sign( signer );
      FC_ASSERT( blk.validate_signee( pub_key ) );
   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}


BOOST_AUTO_TEST_CASE( wallet_test )
{
      fc::temp_directory dir; 

      chain_database_ptr blockchain = std::make_shared<chain_database>();
      blockchain->open( dir.path() );

      wallet  my_wallet( blockchain );
      my_wallet.set_data_directory( dir.path() );
      my_wallet.create(  "my_wallet", "password" );

      my_wallet.close();
      my_wallet.open( "my_wallet", "password" );
      my_wallet.unlock( "password" );
      my_wallet.import_private_key( fc::variant("dce167e01dfd6904015a8106e0e1470110ef2d5b0b18ba7a83cb8204e25c6b5f").as<fc::ecc::private_key>() );
      my_wallet.close();
      my_wallet.open( "my_wallet", "password" );
}


BOOST_AUTO_TEST_CASE( genesis_block_test )
{
   try {
      fc::temp_directory dir2; 
      fc::temp_directory dir; 

      chain_database_ptr blockchain = std::make_shared<chain_database>();
      blockchain->open( dir.path() );

      chain_database_ptr blockchain2 = std::make_shared<chain_database>();
      blockchain2->open( dir2.path() );

      ilog( "." );
      auto delegate_list = blockchain->get_delegates_by_vote();
      ilog( "delegates: ${delegate_list}", ("delegate_list",delegate_list) );
      for( uint32_t i = 0; i < delegate_list.size(); ++i )
      {
         auto name_rec = blockchain->get_name_record( delegate_list[i] );
         ilog( "${i}] ${delegate}", ("i",i) ("delegate", name_rec ) );
      }
      blockchain->scan_names( [=]( const name_record& a ) {
         ilog( "\nname: ${a}", ("a",fc::json::to_pretty_string(a) ) );
      });

      blockchain->scan_assets( [=]( const asset_record& a ) {
         ilog( "\nasset: ${a}", ("a",fc::json::to_pretty_string(a) ) );
      });


      wallet  my_wallet( blockchain );
      my_wallet.set_data_directory( dir.path() );
      my_wallet.create(  "my_wallet", "password" );
      my_wallet.unlock( "password", fc::seconds( 10000000 ) );

      wallet  your_wallet( blockchain2 );
      your_wallet.set_data_directory( dir2.path() );
      your_wallet.create(  "your_wallet", "password" );
      your_wallet.unlock( "password", fc::seconds( 10000000 ) );

      auto keys = fc::json::from_string( test_keys ).as<std::vector<fc::ecc::private_key> >();
      for( auto key: keys )
      {
         my_wallet.import_private_key( key );
      }
      my_wallet.scan_state();
      my_wallet.close();
      my_wallet.open( "my_wallet", "password" );
      my_wallet.unlock( "password", fc::seconds( 1000000 ) );
   //   my_wallet.scan_state();

         ilog( "my balance: ${my}   your balance: ${your}",
               ("my",my_wallet.get_balance("*",0))
               ("your",your_wallet.get_balance("*",0)) );

      ilog( "." );
      share_type total_sent = 0;
      for( uint32_t i = 10; i < 18; ++i )
      {
         auto next_block_time = my_wallet.next_block_production_time();
         ilog( "next block production time: ${t}", ("t",next_block_time) );

         auto wait_until_time = my_wallet.next_block_production_time();
         auto sleep_time = wait_until_time - fc::time_point::now();
         ilog( "waiting: ${t}s", ("t",sleep_time.count()/1000000) );
         fc::usleep( sleep_time );


         full_block next_block = blockchain->generate_block( next_block_time );
         my_wallet.sign_block( next_block );
         ilog( "\n\n\n                   MY_WALLET   PUSH_BLOCK" );
         blockchain->push_block( next_block );
         ilog( "\n\n\n                   YOUR_WALLET   PUSH_BLOCK" );
         blockchain2->push_block( next_block );

         ilog( "my balance: ${my}   your balance: ${your}",
               ("my",my_wallet.get_balance("*",0))
               ("your",your_wallet.get_balance("*",0)) );

         ilog( "\n\n" );
         FC_ASSERT( total_sent == your_wallet.get_balance("*",0).amount, "",
                    ("toatl_sent",total_sent)("balance",your_wallet.get_balance("*",0).amount));

         fc::usleep( fc::microseconds(1200000) );

         for( uint64_t t = 1; t <= 2; ++t )
         {
            std::string your_account_name = "my-"+fc::to_string(i*t);
            auto your_account = your_wallet.create_receive_account( your_account_name ).extended_key;
            my_wallet.create_sending_account( your_account_name, your_account );
            auto amnt = rand()%30000;
            auto invoice_sum = my_wallet.transfer( your_account_name, asset( amnt ) );
            for( auto trx : invoice_sum.payments )
            {
               blockchain->store_pending_transaction( trx.second );
               blockchain2->store_pending_transaction( trx.second );
               ilog( "trx: ${trx}", ("trx",trx.second) );
            }
            total_sent += amnt;
         }

      }

      blockchain->close();
   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
   catch ( ... )
   {
      elog( "exception" );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( basic_fork_test )
{
   try {
    // the purpose of this test is to validate the fork handling
    // when two alternative forks are produced and then merged.

    fc::temp_directory my_dir; 
    fc::temp_directory your_dir; 

    chain_database_ptr my_chain = std::make_shared<chain_database>();
    my_chain->open( my_dir.path() );

    chain_database_ptr your_chain = std::make_shared<chain_database>();
    your_chain->open( your_dir.path() );

    wallet  my_wallet( my_chain );
    my_wallet.set_data_directory( my_dir.path() );
    my_wallet.create(  "my_wallet", "password" );
    my_wallet.unlock( "password", fc::seconds( 10000000 ) );

    wallet  your_wallet( your_chain );
    your_wallet.set_data_directory( your_dir.path() );
    your_wallet.create(  "your_wallet", "password" );
    your_wallet.unlock( "password", fc::seconds( 10000000 ) );


    auto keys = fc::json::from_string( test_keys ).as<std::vector<fc::ecc::private_key> >();
    for( uint32_t i = 0; i < keys.size(); ++i )
    {
       if( i % 3 == 0 )
          my_wallet.import_private_key( keys[i] );
       else
          your_wallet.import_private_key( keys[i] );
    }
    my_wallet.scan_state();
    your_wallet.scan_state();


    // produce blocks for 30 seconds...
    for( uint32_t i = 0; i < 10; ++i )
    {
       auto now = fc::time_point::now();
       std::cerr << "now: "<< std::string( now ) << "\n";
       auto my_next_block_time = my_wallet.next_block_production_time();
       if( my_next_block_time == now )
       {
          auto my_block = my_chain->generate_block( my_next_block_time );
          std::cerr << "producing my block: "<< my_block.block_num <<"\n";
          my_wallet.sign_block( my_block );
          my_chain->push_block( my_block );
          if( i < 5 ) your_chain->push_block( my_block );
       }
       auto your_next_block_time = your_wallet.next_block_production_time();
       if( your_next_block_time == now )
       {
          auto your_block = your_chain->generate_block( your_next_block_time );
          std::cerr << "producing your block: "<< your_block.block_num <<"\n";
          your_wallet.sign_block( your_block );
          your_chain->push_block( your_block );
          if( i < 5 ) my_chain->push_block( your_block );
       }
       auto sleep_time_sec =  BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC  - (now.sec_since_epoch() % BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
       std::cerr << "sleeping " << sleep_time_sec << "s \n";
       fc::usleep( fc::seconds( sleep_time_sec ) );
    }

    // we now have two chains of different lengths
    // simulate the two chains syncing up by pushing the blocks in order...

    std::cerr << "synchronizing chains\n";
    uint32_t your_length = your_chain->get_head_block_num();
    std::cerr << "your length: "<<your_length << "\n";
    for( uint32_t i = 1; i <= your_length; ++i )
    {
       try {
          auto b = your_chain->get_block( i );
          my_chain->push_block(b);
          std::cerr << "push block: " << b.block_num << "    my length: " << my_chain->get_head_block_num() << "\n";
       } 
       catch ( const fc::exception& e ) 
       {
          std::cerr << "    exception: " << e.to_string() << "\n";
       }
    }
    FC_ASSERT( my_chain->get_head_block_num() == your_chain->get_head_block_num() );
  } 
   catch ( const fc::exception& e )
  {
     elog( "${e}", ("e",e.to_detail_string() ) );
     throw;
  }
}
