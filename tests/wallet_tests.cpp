#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
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

      auto your_client = std::make_shared<client>(network);
      your_client->open( your_dir.path(), "genesis.json" );

      my_client->wallet_create( "my_wallet", password );
      my_client->wallet_unlock( fc::seconds(999999999), password );
      your_client->wallet_create( "your_wallet", password );
      your_client->wallet_unlock( fc::seconds(999999999), password );

      auto my_account1 = my_client->wallet_account_create( "account1" );
      my_client->wallet_import_private_key( test_keys[0], "account1", true /*rescan*/ );
      auto bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );

      bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      auto trx = my_client->wallet_account_register( "account1", "account1" ); //variant(), false, "account1" );
      ilog( "----" );
      ilog( "${trx}", ("trx",fc::json::to_pretty_string(trx) ) );
      ilog( "----" );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );
      auto recv_accounts = my_client->get_wallet()->list_receive_accounts();
      ilog( "receive accounts: ${r}", ("r",recv_accounts) );

      auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
      bts::blockchain::advance_time( (int32_t)((next_block_production_time - bts::blockchain::now()).count()/1000000) );
      auto b = my_client->get_chain()->generate_block(next_block_production_time);
      my_client->get_wallet()->sign_block( b );
      ilog( "block: ${b}", ("b",b));
      ilog( "\n\nBROADCASTING\n\n" );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );

      ilog( "my_client ${info}", ("info", fc::json::to_pretty_string(my_client->get_info()) ));
      ilog( "your_client ${info}", ("info", fc::json::to_pretty_string(your_client->get_info()) ));
      ilog( "registered_names: ${info}", ("info", fc::json::to_pretty_string(your_client->blockchain_list_registered_accounts("",100)) ));


   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}


