#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/cli/cli.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::client;
using namespace bts::cli;

const char* test_wif_keys = R"([
  "5KYn77SMFximbA7gWoxZxs8VpxnzdnxJyRcA2hv7EziWdNk7cfX",
  "5KK44Qy6MUcBVHzPo3mHuKzzj5fzmRUrKbnnQE2gCYY4yDnsbyJ",
  "5KkUdWJ1VQAssZ3YY6chkG9uaWmhvPhkKsjRtTxV9KbqXfXy8dx",
  "5HwCeJNdh5PCimAZZMwTW4HfbziPbFE4wA5JadXaSRAi4zp3mgA",
  "5KkTHkXCcik9cDKoMo4BPxrojzjmFBwgEMEuCusoV5fdXhwwf19",
  "5JBUAW2EWd2TFfk2tVQmi86dGPejGedN8tNsHDGUJaVyW8bqjaK",
  "5KV5f8Eohi7WmuxWnP4uJzh7oX9zDEYpTbHBx5jFokSEZoUWpbB"
])";

BOOST_AUTO_TEST_CASE( public_key_type_test )
{
   try { 
    auto k1 = fc::ecc::private_key::generate().get_public_key();
    auto k2 = fc::ecc::private_key::generate().get_public_key();
    auto k3 = fc::ecc::private_key::generate().get_public_key();

    FC_ASSERT( public_key_type( std::string( public_key_type(k1) ) ) == k1);
    FC_ASSERT( public_key_type( std::string( public_key_type(k2) ) ) == k2);
    FC_ASSERT( public_key_type( std::string( public_key_type(k3) ) ) == k3);
  } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   }
}

template<typename T>
void produce_block( T my_client )
{
      auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
      bts::blockchain::advance_time( (int32_t)((next_block_production_time - bts::blockchain::now()).count()/1000000) );
      auto b = my_client->get_chain()->generate_block(next_block_production_time);
      my_client->get_wallet()->sign_block( b );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );
}

BOOST_AUTO_TEST_CASE( client_tests )
{
   try {
      std::string password = "123456789";
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;

      auto test_keys = fc::json::from_string( test_wif_keys ).as<vector<string> >();

      auto network = std::make_shared<bts::net::simulated_network>();

      auto my_client = std::make_shared<client>(network);
      my_client->open( my_dir.path(), "genesis.json" );

      auto my_cli = new cli::cli( my_client, std::cout );

      auto your_client = std::make_shared<client>(network);
      your_client->open( your_dir.path(), "genesis.json" );

      my_client->wallet_create( "my_wallet", password );
      my_client->wallet_unlock( fc::seconds(999999999), password );
      your_client->wallet_create( "your_wallet", password );
      your_client->wallet_unlock( fc::seconds(999999999), password );

      auto my_account1 = my_client->wallet_account_create( "account1" );
      my_client->wallet_import_private_key( test_keys[0], string(), true /*rescan*/ );
      auto bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );

      bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      auto trx = my_client->wallet_account_register( "account1", "delegate-0", variant(), true );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );
      auto recv_accounts = my_client->get_wallet()->list_receive_accounts();
      //ilog( "receive accounts: ${r}", ("r",recv_accounts) );

      produce_block( my_client );

      FC_ASSERT( my_client->get_info()["blockchain_head_block_num"].as_int64() == your_client->get_info()["blockchain_head_block_num"].as_int64() );
      FC_ASSERT( your_client->blockchain_list_registered_accounts("account1",1)[0].name == "account1" );

      public_key_type your_account_key = your_client->wallet_account_create( "youraccount" );
      my_client->wallet_add_contact_account( "youraccount", your_account_key );

      for( uint32_t i = 0; i < 20; ++i )
      {
         my_client->wallet_transfer( 50000000+i, "XTS", "delegate-0", "youraccount" );
      }

      produce_block( my_client );

      //auto result = my_client->wallet_list_unspent_balances();
      my_cli->execute_command_line( "wallet_list_unspent_balances" );
      my_cli->execute_command_line( "wallet_account_transaction_history" );
      
      //ilog( "unspent:\n ${r}", ("r", fc::json::to_pretty_string(result)) );

//      ilog( "my_client ${info}", ("info", fc::json::to_pretty_string(my_client->get_info()) ));
//      ilog( "your_client ${info}", ("info", fc::json::to_pretty_string(your_client->get_info()) ));
//      ilog( "registered_names: ${info}", 
                                                                                                                                                                                                                                                                                                                                                                                                                                   

   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}


