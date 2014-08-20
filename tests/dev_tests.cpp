#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include "dev_fixture.hpp"

BOOST_AUTO_TEST_CASE( timetest )
{ 
  auto block_time =  fc::variant( "20140617T024645" ).as<fc::time_point_sec>();
  auto now =  fc::variant( "20140617T024332" ).as<fc::time_point_sec>();
  elog( "delta: ${d}", ("d", (block_time - now).to_seconds() ) );
}

BOOST_FIXTURE_TEST_CASE( basic_commands, chain_fixture )
{ try {
//   disable_logging();
   enable_logging();
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_list_my_accounts" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "unlock 999999999 masterpassword" );
   exec( clienta, "scan 0 100" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "close" );
   exec( clienta, "open walleta" );
   exec( clienta, "unlock 99999999 masterpassword" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "wallet_account_balance delegate31" );
   exec( clienta, "wallet_delegate_set_block_production delegate31 true" );
   exec( clienta, "wallet_delegate_set_block_production delegate33 true" );
   exec(clienta, "wallet_set_transaction_scanning true");
   exec( clienta, "wallet_set_delegate_trust delegate33 true" );
   exec( clienta, "wallet_set_delegate_trust delegate34 true" );
   exec( clienta, "wallet_set_delegate_trust delegate35 true" );
   exec( clienta, "wallet_set_delegate_trust delegate36 true" );
   exec( clienta, "wallet_set_delegate_trust delegate37 true" );
   exec( clienta, "wallet_set_delegate_trust delegate38 true" );
   exec( clienta, "wallet_set_delegate_trust delegate39 true" );

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "info" );
   exec( clientb, "wallet_set_delegate_trust delegate23 true" );
   exec( clientb, "wallet_set_delegate_trust delegate24 true" );
   exec( clientb, "wallet_set_delegate_trust delegate25 true" );
   exec( clientb, "wallet_set_delegate_trust delegate26 true" );
   exec( clientb, "wallet_set_delegate_trust delegate27 true" );
   exec( clientb, "wallet_set_delegate_trust delegate28 true" );
   exec( clientb, "wallet_set_delegate_trust delegate29 true" );

   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "wallet_account_balance" );
   exec( clientb, "wallet_account_balance delegate30" );
   exec( clientb, "unlock 999999999 masterpassword" );
   exec( clientb, "wallet_delegate_set_block_production delegate30 true" );
   exec( clientb, "wallet_delegate_set_block_production delegate32 true" );
   exec(clientb, "wallet_set_transaction_scanning true");

   exec( clientb, "wallet_asset_create BUSD BitUSD delegate30 \"paper bucks\" null 1000000000 10000 true" );
   produce_block(clientb);

   exec(clientb, "wallet_publish_price_feed delegate0 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate2 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate4 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate6 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate8 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate10 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate12 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate14 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate16 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate18 1 BUSD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate20 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate22 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate24 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate26 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate28 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate30 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate32 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate34 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate36 1 BUSD" );
   produce_block(clientb);
   exec(clientb, "wallet_publish_price_feed delegate38 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate40 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate42 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate44 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate46 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate48 1 BUSD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate50 1 BUSD" );
   exec(clientb, "wallet_publish_price_feed delegate52 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate1 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate3 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate5 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate7 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate9 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate11 1 BUSD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate13 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate15 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate17 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate19 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate21 1 BUSD" );
   produce_block(clienta);
   exec(clienta, "wallet_publish_price_feed delegate23 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate25 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate27 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate29 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate31 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate33 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate35 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate37 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate39 1 BUSD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate41 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate43 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate45 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate47 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate49 1 BUSD" );
   exec(clienta, "wallet_publish_price_feed delegate51 1 BUSD" );

   produce_block(clienta);

   exec( clientb, "short delegate30 105 1.05 BUSD" );
   exec( clientb, "short delegate32 10500 0.001 BUSD" );
   //Next line is intended to fail due to overly-high price
   exec( clientb, "short delegate32 300 1000 BUSD" );
   exec( clienta, "ask delegate31 100 XTS .95 BUSD" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec( clienta, "ask delegate31 1000000 XTS .96 BUSD" );
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec( clienta, "ask delegate31 1000000 XTS 1.3 BUSD" );
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec( clienta, "ask delegate31 1000000 XTS 1.3 BUSD" );

   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book BUSD XTS");

   exec(clientb, "wallet_market_order_list BUSD XTS");
   exec(clientb, "wallet_account_transaction_history delegate30");
   exec(clientb, "wallet_account_transaction_history");

   exec(clienta, "wallet_market_order_list BUSD XTS");
   exec(clienta, "wallet_account_transaction_history delegate31");
   exec(clienta, "wallet_account_transaction_history");
   exec(clienta, "balance");
   exec(clientb, "balance");
   exec( clientb, "short delegate32 300 .69 BUSD" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history");
   exec(clientb, "wallet_market_order_list BUSD XTS"); // TODO: this should filter by account
   exec(clientb, "wallet_market_cancel_order XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7");
   produce_block(clientb);
   exec(clientb, "wallet_account_transaction_history delegate32");
   exec(clienta, "balance" );
   exec(clientb, "balance");
   exec(clientb, "wallet_account_transaction_history");
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec(clienta, "wallet_transfer 95 BUSD delegate31 delegate32");
   produce_block(clienta);
   produce_block(clienta);
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history");
   exec(clientb, "balance" );
   exec(clientb, "wallet_market_cover delegate32 5 BUSD XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7" );
   produce_block(clientb);
   produce_block(clientb);
   exec(clientb, "balance" );
   exec(clientb, "wallet_market_cover delegate32 90 BUSD XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7" );
   exec( clienta, "ask delegate31 100 XTS .001 BUSD" );
   produce_block(clientb);
   exec(clientb, "wallet_account_transaction_history delegate32");
   produce_block(clientb);
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history delegate32");
   exec(clientb, "wallet_market_order_list BUSD XTS"); // TODO: this should filter by account
   exec(clientb, "balance" );
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec(clientb, "wallet_market_cancel_order XTS7zGp53nKGbxm6ASmfJrkDyYXmQ9qH6WtE");
   produce_block(clientb);
   exec(clientb, "balance" );
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec(clientb, "wallet_market_order_list BUSD XTS");
   exec(clientb, "wallet_account_transaction_history delegate32");

   return;
   exec(clienta, "wallet_account_transaction_history delegate31");
   exec(clienta, "balance" );

   exec( clientb, "wallet_account_create b-account" );
   exec( clientb, "wallet_account_balance b-account" );

   exec( clientb, "wallet_account_register b-account delegate30 null 100" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "wallet_account_update_registration b-account delegate30 { \"ip\":\"localhost\"} 75" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_my_accounts" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_transfer 33 XTS delegate31 b-account first-memo" );
   exec( clienta, "wallet_account_transaction_history delegate31" );
   exec( clienta, "wallet_account_transaction_history b-account" );
   exec( clienta, "wallet_account_transaction_history" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   produce_block( clientb );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_account_transaction_history delegate31" );
   exec( clienta, "wallet_account_transaction_history b-account" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_create_account c-account" );
   exec( clientb, "wallet_transfer 10 XTS b-account c-account to-me" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "blockchain_list_delegates" );
   exec( clientb, "wallet_set_delegate_trust b-account true" );
   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_transfer 100000 XTS delegate32 c-account to-me" );
   exec( clientb, "wallet_transfer 100000 XTS delegate30 c-account to-me" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_set_delegate_trust b-account true" );
   // TODO: this should throw an exception from the wallet regarding delegate_vote_limit, but it produces
   // the transaction anyway.
   // TODO: before fixing the wallet production side to include multiple outputs and spread the vote,
   // the transaction history needs to show the transaction as an 'error' rather than 'pending' and
   // properly display the reason for the user.
   // TODO: provide a way to cancel transactions that are pending.
   exec( clienta, "wallet_transfer 100000 XTS delegate31 b-account to-b" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   produce_block( clientb );
   exec( clientb, "balance" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "blockchain_list_delegates" );
   exec( clientb, "wallet_asset_create USD Dollar b-account \"paper bucks\" null 1000000000 1000" );
   exec( clientb, "wallet_asset_create GLD Gold b-account \"gram o gold\" null 1000000000 1000" );
   produce_block( clientb );
   exec( clientb, "blockchain_list_assets" );
   exec( clientb, "wallet_asset_issue 20000 USD c-account \"iou\"" );
   exec( clientb, "wallet_asset_issue 1000 GLD c-account \"gld\"" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "wallet_transfer 20 USD c-account delegate31 c-d31" );
   exec( clientb, "wallet_transfer 20 GLD c-account delegate31 c-d31" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_account_transaction_history delegate31" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "balance" );
   exec( clientb, "bid c-account 120 XTS 5.50 USD" );
   exec( clientb, "bid c-account 20 XTS 6.56 USD" );
   produce_block( clientb );
   exec( clientb, "bid c-account 10 XTS 7.76 USD" );
   produce_block( clientb );
   exec( clientb, "bid c-account 40 XTS 2.50 USD" );
   produce_block( clientb );
   exec( clientb, "bid c-account 120 XTS 4.50 GLD" );
   exec( clientb, "bid c-account 40 XTS 2.50 GLD" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "balance" );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   auto result = clientb->wallet_market_order_list( "USD", "XTS" );
   exec( clientb, "wallet_market_cancel_order " + string( result[0].market_index.owner ) );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   produce_block( clientb );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );

   result = clientb->wallet_market_order_list( "USD", "XTS" );
   exec( clientb, "wallet_market_cancel_order " + string( result[0].market_index.owner ) );
   produce_block( clientb );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_change_passphrase newmasterpassword" );
   exec( clientb, "close" );
   exec( clientb, "open walletb" );
   exec( clientb, "unlock 99999999 newmasterpassword" );
   exec( clientb, "blockchain_get_transaction d387d39ca1" );

   exec( clientb, "wallet_transfer 20 USD c-account delegate31 c-d31" );
   exec( clientb, "blockchain_list_pending_transactions" );
   enable_logging();
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   disable_logging();
   for( uint32_t i = 0; i < 100; ++i )
   {
//      exec( clientb, "wallet_transfer 10 XTS delegate32 delegate32 " );
      produce_block( clientb );
   }
   exec( clientb, "blockchain_get_account delegate32" );
   exec( clientb, "wallet_delegate_withdraw_pay delegate32 c-account .01234" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history delegate32" );
   exec( clientb, "blockchain_list_delegates" );


   exec( clienta, "wallet_set_delegate_trust delegate33 false" );
   exec( clienta, "wallet_set_delegate_trust delegate34 false" );
   exec( clienta, "wallet_set_delegate_trust delegate35 false" );
   exec( clienta, "wallet_set_delegate_trust delegate36 false" );
   exec( clienta, "wallet_set_delegate_trust delegate37 false" );
   exec( clienta, "wallet_set_delegate_trust delegate38 false" );
   exec( clienta, "wallet_set_delegate_trust delegate39 false" );


   exec( clientb, "wallet_set_delegate_trust delegate23 false" );
   exec( clientb, "wallet_set_delegate_trust delegate24 false" );
   exec( clientb, "wallet_set_delegate_trust delegate25 false" );
   exec( clientb, "wallet_set_delegate_trust delegate26 false" );
   exec( clientb, "wallet_set_delegate_trust delegate27 false" );
   exec( clientb, "wallet_set_delegate_trust delegate28 false" );
   exec( clientb, "wallet_set_delegate_trust delegate29 false" );

   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_set_delegate_trust delegate44 true" );
   exec( clienta, "wallet_set_delegate_trust delegate44 true" );
   exec( clienta, "wallet_set_delegate_trust delegate45 true" );
   exec( clienta, "wallet_set_delegate_trust delegate46 true" );
   exec( clienta, "wallet_set_delegate_trust delegate47 true" );
   exec( clienta, "wallet_set_delegate_trust delegate48 true" );
   exec( clienta, "wallet_set_delegate_trust delegate49 true" );

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_set_delegate_trust delegate63 true" );
   exec( clientb, "wallet_set_delegate_trust delegate64 true" );
   exec( clientb, "wallet_set_delegate_trust delegate65 true" );
   exec( clientb, "wallet_set_delegate_trust delegate66 true" );
   exec( clientb, "wallet_set_delegate_trust delegate67 true" );
   exec( clientb, "wallet_set_delegate_trust delegate68 true" );
   exec( clientb, "wallet_set_delegate_trust delegate69 true" );
   exec( clientb, "balance" );
   exec( clienta, "balance" );
   exec( clienta, "wallet_transfer 10691976.59801 XTS delegate31 delegate31 change_votes " );
   exec( clienta, "wallet_transfer 10801980.09801  XTS delegate33 delegate33 change_votes " );
   exec( clientb, "wallet_transfer 9792.18499 XTS b-account b-account change_votes " );
   exec( clientb, "wallet_transfer 20000.40123 XTS c-account c-account change_votes " );
   exec( clientb, "wallet_transfer 10791970.09801 XTS delegate32 delegate32 change_votes " );
   exec( clientb, "wallet_transfer 10791760.18284 XTS delegate30 delegate30 change_votes " );

   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   exec( clienta, "balance" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "blockchain_list_delegates" );

   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_transfer 10691976.59801 XTS delegate31 delegate31 change_votes " );
   exec( clienta, "wallet_transfer 10801980.09801  XTS delegate33 delegate33 change_votes " );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_transfer 9792.18499 XTS b-account b-account change_votes " );
   exec( clientb, "wallet_transfer 20000.40123 XTS c-account c-account change_votes " );
   exec( clientb, "wallet_transfer 10791970.09801 XTS delegate32 delegate32 change_votes " );
   exec( clientb, "wallet_transfer 10791760.18284 XTS delegate30 delegate30 change_votes " );

   exec( clientb, "info" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "info" );

   enable_logging();
   wlog( "FORKING NETWORKS" );
   clientb->simulate_disconnect(true);
   produce_block(clienta);
   produce_block(clienta);
   produce_block(clienta);
   produce_block(clienta);
   produce_block(clienta);
   wlog( "------------------  CLIENT B  -----------------------------------" );
   clientb->simulate_disconnect(false);

   wlog( "------------------  CLIENT A  -----------------------------------" );
   clienta->simulate_disconnect(true);
   wlog( "------------------  CLIENT B  -----------------------------------" );
   produce_block(clientb);
   produce_block(clientb);
   produce_block(clientb);

   wlog( "------------------  CLIENT A  -----------------------------------" );
   clienta->simulate_disconnect(false);
   produce_block(clienta);
   produce_block(clienta);
   produce_block(clienta);

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "info" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "info" );

   wlog( "JOINING NETWORKS" );
   for( uint32_t i = 2; i <= clienta->get_chain()->get_head_block_num(); ++i )
   {
      auto b = clienta->get_chain()->get_block( i );
      clientb->get_chain()->push_block(b);
   }

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "info" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "info" );

   exec( clientb, "wallet_account_update_registration b-account delegate30 { \"ip\":\"localhost\"} 85" );
   exec( clientb, "wallet_account_update_registration b-account delegate30 { \"ip\":\"localhost\"} 65" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "ask c-account 120 XTS 5.00 USD" );
   exec( clientb, "ask c-account 213 XTS 5.67 USD" );
   exec( clientb, "ask c-account 345 XTS 4.56 USD" );
   exec( clientb, "ask c-account 120 XTS 8.00 GLD" );
   exec( clientb, "ask c-account 213 XTS 7.67 GLD" );
   exec( clientb, "ask c-account 345 XTS 6.56 GLD" );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clienta, "info" );
   produce_block( clienta );
   exec( clienta, "info" );
   exec( clienta, "blockchain_list_market_transactions 127" );
   exec( clienta, "blockchain_list_market_transactions 128" );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "blockchain_market_list_shorts USD" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );

   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clienta, "blockchain_market_order_book GLD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_asset_create BUSD BitUSD delegate30 \"paper bucks\" null 1000000000 1000 true" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "short delegate30 3000 5.43 BUSD" );
   exec( clientb, "ask delegate30 400 XTS 5.41 BUSD" );
   exec( clientb, "ask delegate32 800 XTS 4.20 BUSD" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clienta, "blockchain_market_list_shorts BUSD" );
   exec( clientb, "blockchain_market_list_covers BUSD" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list BUSD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "ask delegate30 3 XTS 5.42 BUSD" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "short c-account 50 3.11 BUSD" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list BUSD XTS" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list BUSD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_market_order_list BUSD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "cover delegate32 10.1 BUSD XTSP8ZBZodbzPYh57Z8S4S6x2VqYNXo5MALy" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "wallet_market_order_list BUSD XTS" );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "cover delegate32 19.899 BUSD XTSP8ZBZodbzPYh57Z8S4S6x2VqYNXo5MALy" );
   produce_block( clienta );
   exec( clientb, "balance" );
   exec( clienta, "blockchain_market_order_book BUSD XTS" );

   exec(clientb, "balance");
   exec(clientb, "history");

   exec(clientb, "balance b-account");
   exec(clientb, "history b-account");

   exec(clientb, "balance c-account");
   exec(clientb, "history c-account");
} FC_LOG_AND_RETHROW() }

#if 0
BOOST_FIXTURE_TEST_CASE( malicious_trading, chain_fixture )
{ try {
   return;
   exec( clienta, "wallet_list_my_accounts" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "unlock 999999999 masterpassword" );
   exec( clienta, "scan 0 100" );
   exec( clienta, "wallet_delegate_set_block_production delegate31 true" );
   exec( clienta, "wallet_delegate_set_block_production delegate33 true" );
   exec( clientb, "unlock 999999999 masterpassword" );
   exec( clientb, "wallet_delegate_set_block_production delegate30 true" );
   exec( clientb, "wallet_delegate_set_block_production delegate32 true" );
   exec( clientb, "wallet_account_create b-account" );
   exec( clientb, "wallet_account_balance b-account" );
   exec( clientb, "wallet_asset_create BUSD BitUSD delegate30 \"paper bucks\" null 1000000000 1000 true" );
   produce_block(clienta);

   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "ask delegate21 18000000 XTS 1000000 BUSD");
   exec(clientb, "short delegate20 18000000 .001 BUSD");
   exec(clienta, "ask delegate23 18000000 XTS 1000000 BUSD");
   exec(clientb, "short delegate22 18000000 .001 BUSD");
   exec(clienta, "ask delegate25 18000000 XTS 1000000 BUSD");
   exec(clientb, "short delegate24 18000000 .001 BUSD");
   exec(clienta, "ask delegate27 18000000 XTS 1000000 BUSD");
   exec(clientb, "short delegate26 18000000 .001 BUSD");
   exec(clienta, "ask delegate29 18000000 XTS 1000000 BUSD");
   exec(clientb, "short delegate28 18000000 .001 BUSD");
   exec(clienta, "ask delegate31 18000000 XTS 1.05 BUSD");
   exec(clientb, "short delegate30 18000000 1 BUSD");

   exec(clienta, "ask delegate33 100 XTS .001 BUSD");
   exec(clientb, "short delegate32 100000000 1000000000 BUSD");

   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "bid delegate23 1000000 XTS 5 BUSD");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");

   exec(clienta, "ask delegate31 100 XTS 4 BUSD");

   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");

   exec(clientb, "balance");
   exec(clientb, "history");

   exec(clientb, "balance delegate22");
   exec(clientb, "history delegate22");

   exec(clientb, "balance delegate32");
   exec(clientb, "history delegate32");
   exec(clientb, "wallet_publish_price_feed delegate22 .86 BUSD" );
   produce_block(clienta);
   exec( clientb, "ask delegate22 3 XTS 0.92 BUSD" );
   exec( clientb, "ask delegate22 4 XTS 0.22 BUSD" );
   exec( clientb, "short delegate22 4 2.0 BUSD" );
   enable_logging();
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book BUSD XTS");
   exec( clienta, "wallet_account_transaction_history" );

} FC_LOG_AND_RETHROW() }
#endif

