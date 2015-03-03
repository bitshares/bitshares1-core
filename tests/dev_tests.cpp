#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include "dev_fixture.hpp"


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
   exec( clienta, "wallet_account_set_approval delegate33 true" );
   exec( clienta, "wallet_account_set_approval delegate34 true" );
   exec( clienta, "wallet_account_set_approval delegate35 true" );
   exec( clienta, "wallet_account_set_approval delegate36 true" );
   exec( clienta, "wallet_account_set_approval delegate37 true" );
   exec( clienta, "wallet_account_set_approval delegate38 true" );
   exec( clienta, "wallet_account_set_approval delegate39 true" );

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "info" );
   exec( clientb, "wallet_set_transaction_scanning true");
   exec( clientb, "wallet_account_set_approval delegate23 true" );
   exec( clientb, "wallet_account_set_approval delegate24 true" );
   exec( clientb, "wallet_account_set_approval delegate25 true" );
   exec( clientb, "wallet_account_set_approval delegate26 true" );
   exec( clientb, "wallet_account_set_approval delegate27 true" );
   exec( clientb, "wallet_account_set_approval delegate28 true" );
   exec( clientb, "wallet_account_set_approval delegate29 true" );
   exec( clientb, "wallet_delegate_set_block_production delegate23 true" );
   exec( clientb, "wallet_delegate_set_block_production delegate24 true" );

   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "wallet_account_balance" );
   exec( clientb, "wallet_account_balance delegate30" );
   exec( clientb, "unlock 999999999 masterpassword" );
   exec( clientb, "wallet_delegate_set_block_production delegate30 true" );
   exec( clientb, "wallet_delegate_set_block_production delegate32 true" );
   exec(clientb, "wallet_set_transaction_scanning true");
   exec(clienta, "wallet_set_transaction_scanning true");

   exec( clienta, "wallet_account_create b-account" );
   exec( clienta, "wallet_account_register b-account delegate33" );
   produce_block(clienta);
   produce_block(clientb);
   exec( clientb, "balance delegate30");
   //wallet_asset_create <symbol> <asset_name> <issuer_name> <description> <maximum_share_supply> <precision> [public_data] [is_market_issued]
   exec( clientb, "wallet_asset_create USD BitUSD delegate30 \"paper bucks\" 100000000 10000 null true" );
   exec( clientb, "help" );
   produce_block(clientb);

   exec(clientb, "wallet_publish_price_feed delegate0 1 USD" ); //[[\"USD\",1]]" );
   exec(clientb, "wallet_publish_price_feed delegate2 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate4 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate6 1 USD" );
   return;
   exec(clientb, "wallet_publish_price_feed delegate8 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate10 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate12 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate14 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate16 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate18 1 USD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate20 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate22 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate24 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate26 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate28 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate30 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate32 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate34 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate36 1 USD" );
   produce_block(clientb);
   exec(clientb, "wallet_publish_price_feed delegate38 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate40 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate42 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate44 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate46 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate48 1 USD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate50 1 USD" );
   exec(clientb, "wallet_publish_price_feed delegate52 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate1 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate3 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate5 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate7 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate9 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate11 1 USD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate13 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate15 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate17 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate19 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate21 1 USD" );
   produce_block(clienta);
   exec(clienta, "wallet_publish_price_feed delegate23 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate25 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate27 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate29 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate31 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate33 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate35 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate37 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate39 1 USD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate41 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate43 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate45 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate47 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate49 1 USD" );
   exec(clienta, "wallet_publish_price_feed delegate51 1 USD" );

   produce_block(clienta);

//   wallet_transfer_from_with_escrow <amount_to_transfer> <asset_symbol> <paying_account_name> <from_account_name> <to_account_name> <escrow_account_name> <agreement> <memo>
   exec( clientb, "wallet_transfer_from_with_escrow 1000 XTS delegate24 delegate24 b-account delegate36 \"5100000000000000000000000000000000000000000000000000000000000005\" \"my memo\"" );
   produce_block(clientb);
   produce_block(clienta);
   exec( clientb, "wallet_escrow_summary delegate24" );
   exec( clienta, "wallet_escrow_summary b-account" );
   exec( clienta, "wallet_escrow_summary delegate36" );
   exec( clienta, "wallet_escrow_summary delegate29" );
   exec( clienta, "history" );
   exec( clientb, "history" );
   exec( clientb, "balance delegate24" );
   exec( clienta, "balance b-account" );
   exec( clientb, "wallet_release_escrow delegate24 XTS6AwRFEQ5nu8d2B14qVujPAJDN73bTKgGb sender 0 1111111" );
   //exec( clientb, "wallet_release_escrow delegate24 XTS3UQU85qXeCa7hAP4ps118jSN3cAMSj2bx sender 0 1111111" );
   produce_block(clientb);
   exec( clientb, "wallet_escrow_summary delegate24" );
   exec( clienta, "wallet_escrow_summary b-account" );
   exec( clientb, "balance delegate24" );
   exec( clienta, "balance b-account" );
   produce_block(clientb);
   exec( clienta, "wallet_release_escrow b-account XTS6AwRFEQ5nu8d2B14qVujPAJDN73bTKgGb receiver 3333333 0" );
   produce_block(clienta);
   exec( clientb, "wallet_escrow_summary delegate24" );
   exec( clienta, "wallet_escrow_summary b-account" );
   exec( clientb, "balance delegate24" );
   // wallet_market_submit_relative_bid <from_account_name> <quantity> <quantity_symbol> <relative_price> <base_symbol> [limit_price]
   exec( clienta, "balance b-account" );
   exec( clienta, "blockchain_list_assets" );

   produce_block(clienta);
   exec(clienta, "balance" );
   exec(clientb, "balance" );
   exec( clienta, "wallet_release_escrow delegate36 XTS6AwRFEQ5nu8d2B14qVujPAJDN73bTKgGb receiver 1234 4567" );
   produce_block(clienta);
   exec( clientb, "wallet_escrow_summary delegate24" );
   exec( clienta, "wallet_escrow_summary b-account" );
   return;

   exec( clienta, "wallet_market_submit_relative_ask b-account 1 XTS .0001 USD .02" );
   exec( clienta, "wallet_market_submit_relative_ask b-account 4 XTS .0005 USD 1.3" );
   produce_block(clientb);
   produce_block(clientb);
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clienta, "blockchain_market_list_asks USD XTS" );

   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clienta, "blockchain_market_list_asks USD XTS" );
   exec( clienta, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "short delegate30 200 XTS 2 USD 1.01" );
   exec( clientb, "short delegate30 300 XTS 1.5 USD .99" );
   exec( clientb, "short delegate32 100 XTS 0.45 USD " );
   exec( clienta, "ask delegate31 100 XTS .1997 USD" );
   exec( clienta, "ask delegate31 200 XTS .9998 USD" );
   exec( clienta, "ask delegate31 300 XTS .9999 USD" );
   produce_block(clientb);
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "blockchain_market_list_shorts USD");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "blockchain_market_list_shorts USD");
   exec(clienta, "balance delegate31" );
   exec(clientb, "balance delegate30" );
   exec(clientb, "balance delegate32" );
   exec( clienta, "wallet_market_submit_relative_bid delegate31 1 XTS .0002 USD 2" );
   exec( clienta, "wallet_market_submit_relative_bid delegate31 3 XTS .0006 USD .7" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clientb, "blockchain_market_list_bids USD XTS" );
   return;
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "blockchain_market_list_shorts USD");
   exec(clienta, "blockchain_market_list_covers USD");
   exec( clienta, "ask delegate31 10 XTS .98 USD" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "balance" );
   exec( clienta, "bid delegate31 40 XTS .67 USD" );
   exec( clienta, "bid delegate31 50 XTS .68 USD" );
   exec( clienta, "bid delegate31 37 XTS .741 USD" );
   produce_block(clientb);
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "blockchain_market_order_history USD XTS");

   exec(clientb, "wallet_publish_price_feed delegate0 .74 USD" ); //[[\"USD\",1]]" );
   exec(clientb, "wallet_publish_price_feed delegate2 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate4 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate6 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate8 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate10 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate12 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate14 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate16 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate18 .74 USD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate20 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate22 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate24 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate26 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate28 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate30 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate32 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate34 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate36 .74 USD" );
   produce_block(clientb);
   exec(clientb, "wallet_publish_price_feed delegate38 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate40 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate42 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate44 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate46 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate48 .74 USD" );
   produce_block(clienta);
   exec(clientb, "wallet_publish_price_feed delegate50 .74 USD" );
   exec(clientb, "wallet_publish_price_feed delegate52 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate1 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate3 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate5 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate7 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate9 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate11 .74 USD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate13 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate15 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate17 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate19 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate21 .74 USD" );
   produce_block(clienta);
   exec(clienta, "wallet_publish_price_feed delegate23 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate25 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate27 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate29 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate31 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate33 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate35 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate37 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate39 .74 USD" );
   produce_block(clientb);
   exec(clienta, "wallet_publish_price_feed delegate41 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate43 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate45 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate47 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate49 .74 USD" );
   exec(clienta, "wallet_publish_price_feed delegate51 .74 USD" );
   produce_block(clientb);


   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_history USD XTS");
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "blockchain_get_asset USD XTS");
   exec(clienta, "blockchain_calculate_debt USD");
   exec(clienta, "blockchain_calculate_supply USD");
   exec(clienta, "balance" );
   exec(clientb, "balance" );
   exit(1);
   return;
   elog( "=====================================================================\n" );
   elog( "=====================================================================\n" );
   elog( "=====================================================================\n" );
   elog( "=====================================================================\n" );
   elog( "=====================================================================\n" );
   exec(clientb, "blockchain_market_order_book USD XTS");

   exec( clienta, "short delegate35 4000 USD 30" );
   exec( clienta, "short delegate37 5000 USD 40" );
   exec( clienta, "short delegate39 4000 USD 50" );

   exec( clientb, "ask delegate38 5000 XTS .739 USD" );
   exec( clientb, "ask delegate40 5000 XTS .74 USD" );
   exec( clientb, "ask delegate42 5000 XTS .741 USD" );

   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "balance");
   exec(clientb, "balance");
   exec(clienta, "blockchain_get_asset USD");
   exec(clienta, "blockchain_market_order_book USD XTS");




   return;
   //Next line is intended to fail due to overly-high price
   exec( clientb, "short delegate32 300 1000 USD" );
   exec( clienta, "ask delegate31 100 XTS .95 USD" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec( clienta, "ask delegate31 1000000 XTS .96 USD" );
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec( clienta, "ask delegate31 1000000 XTS 1.3 USD" );
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec( clienta, "ask delegate31 1000000 XTS 1.3 USD" );

   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");

   exec(clientb, "wallet_market_order_list USD XTS");
   exec(clientb, "wallet_account_transaction_history delegate30");
   exec(clientb, "wallet_account_transaction_history");

   exec(clienta, "wallet_market_order_list USD XTS");
   exec(clienta, "wallet_account_transaction_history delegate31");
   exec(clienta, "wallet_account_transaction_history");
   exec(clienta, "balance");
   exec(clientb, "balance");
   exec( clientb, "short delegate32 300 .69 USD" );
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clientb);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history");
   exec(clientb, "wallet_market_order_list USD XTS"); // TODO: this should filter by account
   exec(clientb, "wallet_market_cancel_order XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7");
   produce_block(clientb);
   exec(clientb, "wallet_account_transaction_history delegate32");
   exec(clienta, "balance" );
   exec(clientb, "balance");
   exec(clientb, "wallet_account_transaction_history");
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "wallet_transfer 95 USD delegate31 delegate32");
   produce_block(clienta);
   produce_block(clienta);
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history");
   exec(clientb, "balance" );
   exec(clientb, "wallet_market_cover delegate32 5 USD XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7" );
   produce_block(clientb);
   produce_block(clientb);
   exec(clientb, "balance" );
   exec(clientb, "wallet_market_cover delegate32 90 USD XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7" );
   exec( clienta, "ask delegate31 100 XTS .001 USD" );
   produce_block(clientb);
   exec(clientb, "wallet_account_transaction_history delegate32");
   produce_block(clientb);
   exec(clienta, "wallet_account_transaction_history");
   exec(clientb, "wallet_account_transaction_history delegate32");
   exec(clientb, "wallet_market_order_list USD XTS"); // TODO: this should filter by account
   exec(clientb, "balance" );
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clientb, "wallet_market_cancel_order XTS7zGp53nKGbxm6ASmfJrkDyYXmQ9qH6WtE");
   produce_block(clientb);
   exec(clientb, "balance" );
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clientb, "wallet_market_order_list USD XTS");
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
   exec( clientb, "wallet_account_create c-account" );
   exec( clientb, "wallet_transfer 10 XTS b-account c-account to-me" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "blockchain_list_delegates" );
   exec( clientb, "wallet_account_set_approval b-account true" );
   exec( clientb, "wallet_list_my_accounts" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_transfer 100000 XTS delegate32 c-account to-me" );
   exec( clientb, "wallet_transfer 100000 XTS delegate30 c-account to-me" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_account_set_approval b-account true" );
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
   exec( clientb, "wallet_market_cancel_order " + string( result.begin()->first ) );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   produce_block( clientb );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );

   result = clientb->wallet_market_order_list( "USD", "XTS" );
   exec( clientb, "wallet_market_cancel_order " + string( result.begin()->first ) );
   produce_block( clientb );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_change_passphrase \"newmasterpassword\"" );
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


   exec( clienta, "wallet_account_set_approval delegate33 false" );
   exec( clienta, "wallet_account_set_approval delegate34 false" );
   exec( clienta, "wallet_account_set_approval delegate35 false" );
   exec( clienta, "wallet_account_set_approval delegate36 false" );
   exec( clienta, "wallet_account_set_approval delegate37 false" );
   exec( clienta, "wallet_account_set_approval delegate38 false" );
   exec( clienta, "wallet_account_set_approval delegate39 false" );


   exec( clientb, "wallet_account_set_approval delegate23 false" );
   exec( clientb, "wallet_account_set_approval delegate24 false" );
   exec( clientb, "wallet_account_set_approval delegate25 false" );
   exec( clientb, "wallet_account_set_approval delegate26 false" );
   exec( clientb, "wallet_account_set_approval delegate27 false" );
   exec( clientb, "wallet_account_set_approval delegate28 false" );
   exec( clientb, "wallet_account_set_approval delegate29 false" );

   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_account_set_approval delegate44 true" );
   exec( clienta, "wallet_account_set_approval delegate44 true" );
   exec( clienta, "wallet_account_set_approval delegate45 true" );
   exec( clienta, "wallet_account_set_approval delegate46 true" );
   exec( clienta, "wallet_account_set_approval delegate47 true" );
   exec( clienta, "wallet_account_set_approval delegate48 true" );
   exec( clienta, "wallet_account_set_approval delegate49 true" );

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_account_set_approval delegate63 true" );
   exec( clientb, "wallet_account_set_approval delegate64 true" );
   exec( clientb, "wallet_account_set_approval delegate65 true" );
   exec( clientb, "wallet_account_set_approval delegate66 true" );
   exec( clientb, "wallet_account_set_approval delegate67 true" );
   exec( clientb, "wallet_account_set_approval delegate68 true" );
   exec( clientb, "wallet_account_set_approval delegate69 true" );
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
   exec( clientb, "wallet_asset_create USD BitUSD delegate30 \"paper bucks\" null 1000000000 1000 true" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "short delegate30 3000 5.43 USD" );
   exec( clientb, "ask delegate30 400 XTS 5.41 USD" );
   exec( clientb, "ask delegate32 800 XTS 4.20 USD" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clienta, "blockchain_market_list_shorts USD" );
   exec( clientb, "blockchain_market_list_covers USD" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list USD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "ask delegate30 3 XTS 5.42 USD" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "short c-account 50 3.11 USD" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list USD XTS" );
   produce_block( clienta );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "balance" );
   exec( clienta, "wallet_market_order_list USD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "cover delegate32 10.1 USD XTSP8ZBZodbzPYh57Z8S4S6x2VqYNXo5MALy" );
   produce_block( clienta );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clienta, "blockchain_market_order_book USD XTS" );
   exec( clientb, "balance" );
   exec( clientb, "cover delegate32 19.899 USD XTSP8ZBZodbzPYh57Z8S4S6x2VqYNXo5MALy" );
   produce_block( clienta );
   exec( clientb, "balance" );
   exec( clienta, "blockchain_market_order_book USD XTS" );

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
   exec( clientb, "wallet_asset_create USD BitUSD delegate30 \"paper bucks\" null 1000000000 1000 true" );
   produce_block(clienta);

   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "ask delegate21 18000000 XTS 1000000 USD");
   exec(clientb, "short delegate20 18000000 .001 USD");
   exec(clienta, "ask delegate23 18000000 XTS 1000000 USD");
   exec(clientb, "short delegate22 18000000 .001 USD");
   exec(clienta, "ask delegate25 18000000 XTS 1000000 USD");
   exec(clientb, "short delegate24 18000000 .001 USD");
   exec(clienta, "ask delegate27 18000000 XTS 1000000 USD");
   exec(clientb, "short delegate26 18000000 .001 USD");
   exec(clienta, "ask delegate29 18000000 XTS 1000000 USD");
   exec(clientb, "short delegate28 18000000 .001 USD");
   exec(clienta, "ask delegate31 18000000 XTS 1.05 USD");
   exec(clientb, "short delegate30 18000000 1 USD");

   exec(clienta, "ask delegate33 100 XTS .001 USD");
   exec(clientb, "short delegate32 100000000 1000000000 USD");

   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec(clienta, "wallet_account_balance");
   exec(clientb, "wallet_account_balance");

   exec(clienta, "bid delegate23 1000000 XTS 5 USD");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");

   exec(clienta, "ask delegate31 100 XTS 4 USD");

   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");

   exec(clientb, "balance");
   exec(clientb, "history");

   exec(clientb, "balance delegate22");
   exec(clientb, "history delegate22");

   exec(clientb, "balance delegate32");
   exec(clientb, "history delegate32");
   exec(clientb, "wallet_publish_price_feed delegate22 .86 USD" );
   produce_block(clienta);
   exec( clientb, "ask delegate22 3 XTS 0.92 USD" );
   exec( clientb, "ask delegate22 4 XTS 0.22 USD" );
   exec( clientb, "short delegate22 4 2.0 USD" );
   enable_logging();
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
   exec( clienta, "wallet_account_transaction_history" );

} FC_LOG_AND_RETHROW() }
#endif

/*
BOOST_AUTO_TEST_CASE( timetest )
{ 
  auto block_time =  fc::variant( "20140617T024645" ).as<fc::time_point_sec>();
  auto now =  fc::variant( "20140617T024332" ).as<fc::time_point_sec>();
  elog( "delta: ${d}", ("d", (block_time - now).to_seconds() ) );
}
BOOST_FIXTURE_TEST_CASE( fork_testing, chain_fixture )
{
   produce_block(clientb);
   produce_block(clienta);
   exec( clientb, "info" );
   exec( clienta, "info" );
}
*/
