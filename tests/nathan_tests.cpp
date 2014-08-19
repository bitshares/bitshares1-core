#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include "dev_fixture.hpp"

BOOST_FIXTURE_TEST_CASE( printing_dollars, chain_fixture )
{ try {
    enable_logging();
    exec(clienta, "scan");
    exec(clientb, "scan");
    exec(clienta, "wallet_delegate_set_block_production ALL true");
    exec(clientb, "wallet_delegate_set_block_production ALL true");
    exec(clienta, "wallet_set_transaction_scanning true");
    exec(clientb, "wallet_set_transaction_scanning true");
    exec(clienta, "balance");
    exec(clientb, "balance");

    exec(clienta, "wallet_asset_create USD \"Federal Reserve Floats\" delegate21 \"100% Genuine United States Fiat\" \"arbitrary data!\" 1000000000 10000 true");

    produce_block(clienta);

    exec(clientb, "wallet_publish_price_feed delegate22 .02 USD" );
    exec(clientb, "wallet_publish_price_feed delegate24 .02 USD" );
    exec(clientb, "wallet_publish_price_feed delegate26 .02 USD" );
    exec(clientb, "wallet_publish_price_feed delegate28 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate23 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate25 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate27 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate29 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate31 .02 USD" );
    exec(clienta, "wallet_publish_price_feed delegate33 .02 USD" );

    produce_block(clienta);
    std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

    exec(clienta, "short delegate21 10000 .001 USD");
    exec(clientb, "ask delegate20 10000000 XTS .05 USD");

    produce_block(clienta);

    exec(clienta, "short delegate23 100 .01 USD");
    exec(clientb, "ask delegate22 10000 XTS .01 USD");

    produce_block(clienta);
    produce_block(clienta);

    exec(clientb, "wallet_market_cancel_order XTS6qdRRcxniUpYkvo7kBZQVEqyQDTEXC4uV");
    exec(clientb, "ask delegate20 9000000 XTS .005 USD");

    for (int i = 0; i < 360 - 92; ++i)
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

    exec(clienta, "blockchain_get_asset USD");

    exec(clientb, "bid delegate22 25000 XTS .004 USD");

    produce_block(clienta);
    produce_block(clienta);

    exec(clienta, "balance");
    exec(clientb, "balance");
    exec(clienta, "blockchain_market_order_history USD XTS");
    exec(clienta, "blockchain_market_order_book USD XTS");

    exec(clienta, "blockchain_get_asset USD");

    string cmd;
    std::getline(std::cin, cmd);
    while (!cmd.empty()) {
        exec(clienta, cmd);
        exec(clientb, cmd);
        std::getline(std::cin, cmd);
    }

} FC_LOG_AND_RETHROW() }
