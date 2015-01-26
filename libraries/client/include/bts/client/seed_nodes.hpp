#pragma once

namespace bts { namespace client {

static const std::vector<std::string> SEED_NODES
{
    std::string( BTS_NET_TEST_SEED_IP ) + ":" + std::to_string( BTS_NET_TEST_P2P_PORT + BTS_TEST_NETWORK_VERSION )
};

} } // bts::client
