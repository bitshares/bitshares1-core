#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include "dev_fixture.hpp"

class nathan_fixture : public chain_fixture {
public:
    nathan_fixture() {
        enable_logging();
        exec(clienta, "scan");
        exec(clientb, "scan");
        exec(clienta, "wallet_delegate_set_block_production ALL true");
        exec(clientb, "wallet_delegate_set_block_production ALL true");
        exec(clienta, "wallet_set_transaction_scanning true");
        exec(clientb, "wallet_set_transaction_scanning true");

        exec(clienta, "wallet_asset_create USD \"Federal Reserve Floats\" delegate21 \"100% Genuine United States Fiat\" \"arbitrary data!\" 1000000000 10000 true");

        produce_block(clienta);

        exec(clientb, "wallet_publish_price_feed delegate2 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate4 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate6 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate8 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate10 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate12 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate14 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate16 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clientb, "wallet_publish_price_feed delegate18 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate20 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate22 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate24 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate26 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate28 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate30 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate32 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clientb, "wallet_publish_price_feed delegate34 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate36 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate38 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate40 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate42 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate44 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate46 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clientb, "wallet_publish_price_feed delegate48 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate50 .2 USD" );
        exec(clientb, "wallet_publish_price_feed delegate52 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate1 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate3 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate5 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate7 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate9 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clienta, "wallet_publish_price_feed delegate11 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate13 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate15 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate17 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate19 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate21 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate23 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate25 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clienta, "wallet_publish_price_feed delegate27 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate29 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate31 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate33 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate35 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate37 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate39 .2 USD" );
        produce_block(clienta);
        produce_block(clientb);
        exec(clienta, "wallet_publish_price_feed delegate41 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate43 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate45 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate47 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate49 .2 USD" );
        exec(clienta, "wallet_publish_price_feed delegate51 .2 USD" );

        produce_block(clienta);
        produce_block(clientb);
        std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
    }

    void prompt()
    {
        string cmd;
        std::cout << ">>> ";
        std::getline(std::cin, cmd);
        while (!cmd.empty()) {
            exec(clienta, cmd);
            exec(clientb, cmd);
            std::cout << ">>> ";
            std::getline(std::cin, cmd);
        }
    }
};

//BOOST_FIXTURE_TEST_CASE( mail_server, chain_fixture )
//{ try {
//    config aconfig = clienta->configure(clienta_dir.path());
//    aconfig.mail_server_enabled = true;
//    fc::json::save_to_file(aconfig, clienta_dir.path() / "config.json");
//    clienta->open(clienta_dir.path(), clienta_dir.path() / "genesis.json");

//    bts::mail::signed_email_message sem;
//    string subject = "A subject line";
//    string body = "A body\n\nSincerely, Nathan";
//    attachment attach;
//    attach.data = fc::raw::pack(map<string,string>({{string("hello"),string("world")}}));
//    sem.subject = subject;
//    sem.body = body;
//    sem.attachments.push_back(attach);

//    fc::ecc::private_key my_key = fc::ecc::private_key::generate();
//    fc::ecc::private_key one_time_key = fc::ecc::private_key::generate();
//    fc::ecc::private_key his_key = fc::ecc::private_key::generate();

//    sem.sign(my_key);
//    BOOST_CHECK_EQUAL(sem.from().to_base58(), my_key.get_public_key().to_base58());

//    fc::time_point before = fc::time_point::now();
//    message m(message(sem).encrypt(one_time_key, his_key.get_public_key()));
//    clienta->mail_store_message(his_key.get_public_key(), m);
//    fc::time_point after = fc::time_point::now();
//    BOOST_CHECK(after > before);

//    BOOST_CHECK(clienta->mail_fetch_inventory(his_key.get_public_key(), after).size() == 0);
//    auto inventory = clienta->mail_fetch_inventory(his_key.get_public_key(), before);
//    BOOST_CHECK(inventory.size() == 1);

//    message received_message = clienta->mail_fetch_message(inventory[0].second);
//    BOOST_CHECK(received_message.type == encrypted);
//    signed_email_message rm = received_message.as<encrypted_message>().decrypt(his_key).as<signed_email_message>();

//    BOOST_CHECK_EQUAL(rm.from().to_base58(), my_key.get_public_key().to_base58());
//    BOOST_CHECK_EQUAL(rm.subject, subject);
//    BOOST_CHECK_EQUAL(rm.body, body);
//    BOOST_CHECK_EQUAL(rm.attachments.size(), 1);
//    BOOST_CHECK_EQUAL(rm.attachments[0].name, attach.name);
//    BOOST_CHECK(rm.attachments[0].data == attach.data);
//} FC_LOG_AND_RETHROW() }

//BOOST_FIXTURE_TEST_CASE( short_below_feed, nathan_fixture )
//{ try {
//    //This should fail.
//    exec(clienta, "short delegate21 100 USD 5 .194");
//    //This should work.
//    exec(clienta, "short delegate21 100 USD 10 .195");
//    produce_block(clienta);
//    produce_block(clientb);

//    exec(clientb, "blockchain_market_order_book USD XTS");
//    exec(clientb, "blockchain_market_list_shorts USD");
//    exec(clientb, "ask delegate20 10 XTS .19 USD");

//    produce_block(clientb);
//    exec(clientb, "blockchain_market_order_book USD XTS");
//    exec(clientb, "blockchain_market_list_shorts USD");
//    produce_block(clientb);
//    exec(clientb, "blockchain_market_order_book USD XTS");
//    exec(clientb, "blockchain_market_list_shorts USD");

//    prompt();
//} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( to_ugly_asset, nathan_fixture )
{ try {
    chain_interface_ptr chain = clienta->get_chain();

    BOOST_CHECK(chain->to_ugly_asset("100.005", "XTS") == asset(10000500));
    BOOST_CHECK(chain->to_ugly_asset("100", "XTS") == asset(10000000));
    BOOST_CHECK(chain->to_ugly_asset("100.00500", "XTS") == asset(10000500));
    BOOST_CHECK(chain->to_ugly_asset("100.0050000", "XTS") == asset(10000500));
    BOOST_CHECK(chain->to_ugly_asset("100.0050001", "XTS") == asset(10000500));
    BOOST_CHECK(chain->to_ugly_asset("100.", "XTS") == asset(10000000));
    BOOST_CHECK(chain->to_ugly_asset("100.123456", "XTS") == asset(10012345));
    BOOST_CHECK(chain->to_ugly_asset("100.0000001", "XTS") == asset(10000000));
    BOOST_CHECK(chain->to_ugly_asset("-100.005", "XTS") == asset(-10000500));
    BOOST_CHECK(chain->to_ugly_asset("-100", "XTS") == asset(-10000000));
    BOOST_CHECK(chain->to_ugly_asset("-100.00500", "XTS") == asset(-10000500));
    BOOST_CHECK(chain->to_ugly_asset("-100.0050000", "XTS") == asset(-10000500));
    BOOST_CHECK(chain->to_ugly_asset("-100.0050001", "XTS") == asset(-10000500));
    BOOST_CHECK(chain->to_ugly_asset("-100.", "XTS") == asset(-10000000));
    BOOST_CHECK(chain->to_ugly_asset("-100.123456", "XTS") == asset(-10012345));
    BOOST_CHECK(chain->to_ugly_asset("-100.0000001", "XTS") == asset(-10000000));
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( market_stuff, nathan_fixture )
{ try {
   transaction_builder_ptr a_builder = clienta->get_wallet()->create_transaction_builder();
   transaction_builder_ptr b_builder = clientb->get_wallet()->create_transaction_builder();

   wallet_account_record record_21 = clienta->get_wallet()->get_account("delegate21");
   wallet_account_record record_22 = clientb->get_wallet()->get_account("delegate22");

   asset_id_type usd = clienta->get_chain()->get_asset_id("USD");
   asset_id_type xts = 0;

   auto trx = a_builder->submit_short(record_21, asset(1000000, xts), price(.015, usd, xts))
                        .submit_short(record_21, asset(20000000, xts), price(.01, usd, xts))
                        .finalize().sign().trx;

   edump((fc::json::to_pretty_string(trx)));
   clienta->network_broadcast_transaction(trx);

   produce_block(clienta);
   exec(clienta, "blockchain_market_list_shorts USD");
   produce_block(clienta);
   exec(clienta, "blockchain_market_order_book USD XTS");
} FC_LOG_AND_RETHROW() }

/*
BOOST_FIXTURE_TEST_CASE( simultaneous_cancel_buy, nathan_fixture )
{ try {
    //Get sufficient depth to execute trades
    exec(clienta, "short delegate21 10000 .001 USD");
    exec(clientb, "ask delegate20 10000000 XTS .05 USD");

    produce_block(clienta);

    //Give delegate22 some USD; give delegate23 a margin position created at .01 USD/XTS
    exec(clienta, "short delegate23 100 .01 USD");
    exec(clientb, "ask delegate22 10000 XTS .01 USD");

    produce_block(clienta);
    produce_block(clienta);

    exec(clienta, "blockchain_market_order_book USD XTS");

    exec(clienta, "ask delegate23 100 XTS .01 USD");

    produce_block(clienta);
    exec(clientb, "bid delegate22 100 XTS .01 USD");
    produce_block(clienta);

    exec(clienta, "cancel XTSK6TnnRSZymhLfKfLZiL9qLXE3KRmRBrFk");

    produce_block(clienta);
    produce_block(clienta);
    produce_block(clienta);
    produce_block(clienta);
    produce_block(clienta);
    produce_block(clienta);
    produce_block(clienta);

    prompt();
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( rapid_price_change, nathan_fixture )
{ try {
    //Get sufficient depth to execute trades
    exec(clienta, "short delegate21 10000 .001 USD");
    exec(clientb, "ask delegate20 10000000 XTS .05 USD");

    produce_block(clienta);

    //Give delegate22 some USD; give delegate23 a margin position created at .01 USD/XTS
    exec(clienta, "short delegate23 100 .01 USD");
    exec(clientb, "ask delegate22 10000 XTS .01 USD");

    produce_block(clienta);
    produce_block(clienta);

    exec(clientb, "wallet_market_cancel_order XTS6qdRRcxniUpYkvo7kBZQVEqyQDTEXC4uV");

    exec(clientb, "bid delegate22 1 XTS 100 USD true");
    exec(clientb, "ask delegate20 9000000 XTS 11000000 USD");

    produce_block(clienta);
    produce_block(clienta);

    for (int i = 0; i < 360; ++i) {
        exec(clienta, "ask delegate25 .00001 XTS 100 USD");

        produce_block(clienta);
    }
    produce_block(clienta);

    double avg_price = clienta->blockchain_market_status("USD", "XTS").center_price;
    double target_price = avg_price * 2.0 / 3.0;

    exec(clienta, "wallet_market_cancel_order XTS5N1fDwFABDNsP37rTZfsfmkq84eneWotk");
    exec(clientb, "wallet_market_cancel_order XTSAww9Q1cp46eMpfVHpvgPwzMUBfxAHDdFV");
    exec(clientb, "wallet_market_cancel_order XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7");

    exec(clientb, "ask delegate20 9000000 XTS 15 USD true");
    exec(clienta, "short delegate27 .00017 .00000000001 USD");

    produce_block(clienta);

    exec(clienta, "blockchain_market_order_book USD XTS");

    int blocks = 0;
    while (clienta->blockchain_market_status("USD", "XTS").center_price > target_price) {
        exec(clienta, "ask delegate25 .00001 XTS .00000000001 USD");

        produce_block(clienta);
        ++blocks;
    }
    std::cout << "Moved the price by 1/3 (from " << avg_price << " to " << clienta->blockchain_market_status("USD", "XTS").center_price << ") in " << blocks << " blocks.\n";

    exec(clienta, "blockchain_market_order_book USD XTS");

    prompt();
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( printing_dollars, nathan_fixture )
{ try {
    //Get sufficient depth to execute trades
    exec(clienta, "short delegate21 10000 .001 USD");
    exec(clientb, "ask delegate20 10000000 XTS .05 USD");

    produce_block(clienta);

    //Give delegate22 some USD; give delegate23 a margin position created at .01 USD/XTS
    exec(clienta, "short delegate23 100 .01 USD");
    exec(clientb, "ask delegate22 10000 XTS .01 USD");

    produce_block(clienta);
    produce_block(clienta);

    //Drop the lowest ask price way, way down
    exec(clientb, "wallet_market_cancel_order XTS6qdRRcxniUpYkvo7kBZQVEqyQDTEXC4uV");
    exec(clientb, "ask delegate20 9000000 XTS .005 USD");

    //Approximately 1.5 market hours of trading at .002 USD/XTS
    for (int i = 0; i < 360; ++i)
    {
        exec(clienta, "ask delegate23 100 XTS .002 USD");
        exec(clientb, "bid delegate22 100 XTS .002 USD");
        produce_block(clienta);
        produce_block(clienta);
        exec(clientb, "ask delegate22 100 XTS .002 USD");
        exec(clienta, "bid delegate23 100 XTS .002 USD");
        produce_block(clienta);
        produce_block(clienta);
    }

    //Make delegate22 sell his 100 USD at .004 USD/XTS, thereby calling delegate23
    exec(clientb, "bid delegate22 25000 XTS .004 USD");

    produce_block(clienta);
    produce_block(clienta);

    //Observe the carnage
    exec(clienta, "balance");
    exec(clientb, "balance");
    exec(clienta, "blockchain_market_order_history USD XTS");
    exec(clienta, "blockchain_market_order_book USD XTS");
    exec(clienta, "blockchain_get_asset USD");

    //Programmatically verify the carnage
    BOOST_CHECK_EQUAL(clienta->blockchain_get_asset("USD")->collected_fees, -200000);
} FC_LOG_AND_RETHROW() }
*/
