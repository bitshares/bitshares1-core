#include <boost/bind.hpp>
#include <bts/dns/p2p/p2p_rpc_server.hpp>

namespace bts { namespace dns { namespace p2p {

static rpc_server::method_data bid_on_domain_metadata { "bid_on_domain", nullptr,
/* description: */  "Place a bid for an unowned domain name",
/* returns: */      "bool",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true},
                   {"bid_price",    "asset",   true}},
/* prerequisites: */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked };
fc::variant p2p_rpc_server::bid_on_domain(const fc::variants& params)
{
    auto domain_name = params[0].as_string();
    auto bid_price = params[1].as<asset>();

    auto tx = get_dns_wallet()->bid(domain_name, bid_price, get_client()->get_pending_transactions());
    get_client()->broadcast_transaction(tx);

    return fc::variant(true);
}

static rpc_server::method_data auction_domain_metadata { "auction_domain", nullptr,
/* description: */  "Put and owned domain name up for auction",
/* returns: */      "bool",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true},
                   {"ask_price",    "asset",   true}},
/* prerequisites: */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked };
fc::variant p2p_rpc_server::auction_domain(const fc::variants& params)
{
    auto domain_name = params[0].as_string();
    auto ask_price = params[1].as<asset>();

    auto tx = get_dns_wallet()->ask(domain_name, ask_price, get_client()->get_pending_transactions());
    get_client()->broadcast_transaction(tx);

    return fc::variant(true);
}

static rpc_server::method_data transfer_domain_metadata { "transfer_domain", nullptr,
/* description: */  "Transfer a domain name to another address",
/* returns: */      "bool",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true},
                   {"to_address",   "address", true}},
/* prerequisites: */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked };
fc::variant p2p_rpc_server::transfer_domain(const fc::variants& params)
{
    auto domain_name = params[0].as_string();
    auto to_address = params[1].as<address>();

    auto tx = get_dns_wallet()->transfer(domain_name, to_address, get_client()->get_pending_transactions());
    get_client()->broadcast_transaction(tx);

    return fc::variant(true);
}

static rpc_server::method_data update_domain_record_metadata { "update_domain_record", nullptr,
/* description: */  "Update a domain name's record",
/* returns: */      "bool",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true},
                   {"record",       "string",  true}},
/* prerequisites: */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked };
fc::variant p2p_rpc_server::update_domain_record(const fc::variants& params)
{
    auto domain_name = params[0].as_string();
    auto record = params[1].as_string();

    auto tx = get_dns_wallet()->set(domain_name, record, get_client()->get_pending_transactions());
    get_client()->broadcast_transaction(tx);

    return fc::variant(true);
}

static rpc_server::method_data lookup_domain_record_metadata { "lookup_domain_record", nullptr,
/* description: */  "Lookup a domain name's record",
/* returns: */      "string",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true}},
/* prerequisites: */ rpc_server::json_authenticated };
fc::variant p2p_rpc_server::lookup_domain_record(const fc::variants& params)
{
    auto domain_name = params[0].as_string();

    return get_dns_wallet()->lookup(domain_name, get_client()->get_pending_transactions());
}

static rpc_server::method_data name_show_metadata { "name_show", nullptr,
/* description: */  "Lookup a domain name's record",
/* returns: */      "string",
/* params:           name            type      required */
                  {{"domain_name",  "string",  true}},
/* prerequisites: */ rpc_server::json_authenticated };
fc::variant p2p_rpc_server::name_show(const fc::variants& params)
{
    return "{\"value\":{\"ip\":\"192.168.1.1\"}}";
}


static rpc_server::method_data list_active_domain_auctions_metadata { "list_active_domain_auctions", nullptr,
/* description: */  "List currently active domain name auctions",
/* returns: */      "vector<pair<asset, claim_dns_output>>",
/* params:           name            type      required */
                    {},
/* prerequisites: */ rpc_server::json_authenticated };
fc::variant p2p_rpc_server::list_active_domain_auctions(const fc::variants& params)
{
    auto active_domain_auctions = get_dns_wallet()->get_active_auctions();

    std::vector<std::pair<asset, claim_dns_output>> outputs;
    outputs.reserve(active_domain_auctions.size());

    for (auto auction : active_domain_auctions)
        outputs.push_back(std::make_pair(auction.amount, to_dns_output(auction)));

    return fc::variant(outputs);
}

p2p_rpc_server::p2p_rpc_server()
{
#define REGISTER_RPC_METHOD(r, data, METHOD) \
    do { \
        method_data data_with_functor(BOOST_PP_CAT(METHOD, _metadata)); \
        data_with_functor.method = boost::bind(&p2p_rpc_server::METHOD, this, _1); \
        register_method(data_with_functor); \
    } while (0);
#define REGISTER_RPC_METHODS(METHODS) BOOST_PP_SEQ_FOR_EACH(REGISTER_RPC_METHOD, data, METHODS)

    REGISTER_RPC_METHODS(P2P_RPC_METHOD_LIST)

#undef  REGISTER_RPC_METHODS
#undef  REGISTER_RPC_METHOD
}

p2p_rpc_server::~p2p_rpc_server()
{
}

dns_wallet_ptr p2p_rpc_server::get_dns_wallet()
{
    return std::dynamic_pointer_cast<dns_wallet>(get_client()->get_wallet());
}

} } } // bts::dns::p2p
