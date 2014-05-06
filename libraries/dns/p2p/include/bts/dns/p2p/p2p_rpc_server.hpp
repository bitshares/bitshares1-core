#pragma once

#include <bts/dns/dns_wallet.hpp>
#include <bts/rpc/rpc_server.hpp>

#define P2P_RPC_METHOD_LIST \
    (bid_on_domain) \
    (auction_domain) \
    (transfer_domain) \
    (update_domain_record) \
    (lookup_domain_record) \
    (name_show) \
    (list_active_domain_auctions)

namespace bts { namespace dns { namespace p2p {

using namespace bts::rpc;

class p2p_rpc_server : public bts::rpc::rpc_server
{
    private:
#define DECLARE_RPC_METHOD(r, data, METHOD) fc::variant METHOD(const fc::variants&);
#define DECLARE_RPC_METHODS(METHODS) BOOST_PP_SEQ_FOR_EACH(DECLARE_RPC_METHOD, data, METHODS)

        DECLARE_RPC_METHODS(P2P_RPC_METHOD_LIST)

#undef  DECLARE_RPC_METHOD
#undef  DECLARE_RPC_METHODS

    public:
        p2p_rpc_server();
        ~p2p_rpc_server();

        dns_wallet_ptr get_dns_wallet();
};

typedef std::shared_ptr<p2p_rpc_server> p2p_rpc_server_ptr;

} } } // bts::dns::p2p
