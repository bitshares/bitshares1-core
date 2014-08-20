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
        std::getline(std::cin, cmd);
        while (!cmd.empty()) {
            exec(clienta, cmd);
            exec(clientb, cmd);
            std::getline(std::cin, cmd);
        }
    }
};

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

    double avg_price = clienta->blockchain_market_status("USD", "XTS").avg_price_24h;
    double target_price = avg_price * 2.0 / 3.0;

    exec(clienta, "wallet_market_cancel_order XTS5N1fDwFABDNsP37rTZfsfmkq84eneWotk");
    exec(clientb, "wallet_market_cancel_order XTSAww9Q1cp46eMpfVHpvgPwzMUBfxAHDdFV");
    exec(clientb, "wallet_market_cancel_order XTS7FDgYCCxD29WutqJtbvqyvaxdkxYeBVs7");

    exec(clientb, "ask delegate20 9000000 XTS 15 USD true");
    exec(clienta, "short delegate27 .00017 .00000000001 USD");

    produce_block(clienta);

    exec(clienta, "blockchain_market_order_book USD XTS");

    int blocks = 0;
    while (clienta->blockchain_market_status("USD", "XTS").avg_price_24h > target_price) {
        exec(clienta, "ask delegate25 .00001 XTS .00000000001 USD");

        produce_block(clienta);
        ++blocks;
    }
    std::cout << "Moved the price by 1/3 (from " << avg_price << " to " << clienta->blockchain_market_status("USD", "XTS").avg_price_24h << ") in " << blocks << " blocks.\n";

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
