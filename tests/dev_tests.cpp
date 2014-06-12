#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include "dev_fixture.hpp"

BOOST_FIXTURE_TEST_CASE( basic_commands, chain_fixture )
{ try {
//   disable_logging();
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_list_receive_accounts" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "unlock 999999999 masterpassword" );
   exec( clienta, "scan 0 100" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "close" );
   exec( clienta, "open walleta" );
   exec( clienta, "unlock 99999999 masterpassword" );
   exec( clienta, "wallet_account_balance" );
   exec( clienta, "wallet_account_balance delegate31" );

   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_receive_accounts" );
   exec( clientb, "wallet_account_balance" );
   exec( clientb, "wallet_account_balance delegate30" );
   exec( clientb, "unlock 999999999 masterpassword" );

   exec( clientb, "wallet_account_create b-account" );
   exec( clientb, "wallet_account_balance b-account" );

   exec( clientb, "wallet_account_register b-account delegate30" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_receive_accounts" );
   exec( clientb, "wallet_account_update_registration b-account delegate30 { \"ip\":\"localhost\"} true" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_list_receive_accounts" );
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
   exec( clientb, "wallet_set_delegate_trust_level b-account 1" );
   exec( clientb, "wallet_list_receive_accounts" );
   exec( clientb, "balance" );
   exec( clientb, "wallet_transfer 100000 XTS delegate32 c-account to-me" );
   exec( clientb, "wallet_transfer 100000 XTS delegate30 c-account to-me" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_set_delegate_trust_level b-account 1" );
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
   produce_block( clientb );
   exec( clientb, "blockchain_list_registered_assets" );
   exec( clientb, "wallet_asset_issue 1000 USD c-account \"iou\"" );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history b-account" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "wallet_transfer 20 USD c-account delegate31 c-d31" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "wallet_account_transaction_history c-account" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   exec( clienta, "wallet_account_transaction_history delegate31" );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "balance" );
   exec( clientb, "bid c-account 120 XTS 4.50 USD" );
   exec( clientb, "bid c-account 40 XTS 2.50 USD" );
   produce_block( clientb );
   exec( clientb, "wallet_account_transaction_history c-account" );
   exec( clientb, "balance" );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "wallet_market_order_list USD XTS" );
   auto result = clientb->wallet_market_order_list( "USD", "XTS" );
   exec( clientb, "wallet_market_cancel_order " + string( result[0].order.market_index.owner ) );
   produce_block( clientb );
   exec( clientb, "wallet_market_order_list USD XTS" );
   exec( clientb, "blockchain_market_list_bids USD XTS" );
   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "balance" );

   result = clientb->wallet_market_order_list( "USD", "XTS" );
   exec( clientb, "wallet_market_cancel_order " + string( result[0].order.market_index.owner ) );
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
   exec( clientb, "blockchain_get_pending_transactions" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   enable_logging();
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   exec( clientb, "blockchain_get_transaction 6f28bd041522ebf968009b1ff85dcc6355d80cb7" );


   exec( clientb, "wallet_account_transaction_history" );
   exec( clientb, "blockchain_get_transaction 6f28bd04" );
   exec( clientb, "blockchain_list_current_round_active_delegates" );



} FC_LOG_AND_RETHROW() }




