#include <bts/dns/dns_rpc_server.hpp>
#include <bts/dns/dns_wallet.hpp>
#include <boost/bind.hpp>

namespace bts { namespace dns {

  namespace detail
  {
    class dns_rpc_server_impl
    {
    public:
      dns_rpc_server* _self;

      const dns_wallet_ptr get_dns_wallet()
      {
        return std::dynamic_pointer_cast<dns_wallet>(_self->get_client()->get_wallet());
      }
      const dns_db_ptr get_dns_db()
      {
        return std::dynamic_pointer_cast<dns_db>(_self->get_client()->get_chain());
      }

      fc::variant bid_on_domain(const fc::variants& params);
      fc::variant auction_domain(const fc::variants& params);
      fc::variant transfer_domain(const fc::variants& params);
      fc::variant update_domain_record(const fc::variants& params);
      fc::variant list_active_auctions(const fc::variants& params);
      fc::variant lookup_domain_record(const fc::variants& params);
    };

    fc::variant dns_rpc_server_impl::bid_on_domain(const fc::variants& params)
    {
      std::string name = params[0].as_string();
      asset bid = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->bid_on_domain(name, bid, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::auction_domain(const fc::variants& params)
    {
      std::string name = params[0].as_string();
      asset price = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->auction_domain(name, price, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::transfer_domain(const fc::variants& params)
    {
      std::string name = params[0].as_string();
      auto to_owner = params[1].as<bts::blockchain::address>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->transfer_domain(name, to_owner, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::update_domain_record(const fc::variants& params)
    {
      std::string name = params[0].as_string();
      asset bid = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->bid_on_domain(name, bid, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::list_active_auctions(const fc::variants& params)
    {
      std::vector<trx_output> active_auctions = get_active_auctions(*get_dns_db());

      std::vector<std::pair<bts::blockchain::asset, claim_domain_output> > claim_domain_outputs;
      claim_domain_outputs.reserve(active_auctions.size());
      for (const trx_output& output : active_auctions)
      {
        claim_domain_outputs.push_back(std::make_pair(output.amount, to_domain_output(output)));
      }
      return fc::variant(claim_domain_outputs);
    }
    fc::variant dns_rpc_server_impl::lookup_domain_record(const fc::variants& params)
    {
      std::string name = params[0].as_string();
      return lookup_value(name, *get_dns_db());
    }

  } // end namespace detail

  dns_rpc_server::dns_rpc_server() :
    my(new detail::dns_rpc_server_impl)
  {
    my->_self = this;

#define JSON_METHOD_IMPL(METHODNAME) \
    boost::bind(&detail::dns_rpc_server_impl::METHODNAME, my.get(), _1)

    method_data bid_on_domain_metadata{"bid_on_domain", JSON_METHOD_IMPL(bid_on_domain),
                     /* description */ "TODO: describe me",
                     /* returns: */    "bool",
                     /* params:          name            type       required */ 
                                       {{"domain_name",  "string",  true},
                                        {"amount",       "asset",   true}},
                   /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(bid_on_domain_metadata);

    method_data auction_domain_metadata{"auction_domain", JSON_METHOD_IMPL(auction_domain),
                      /* description */ "TODO: describe me",
                      /* returns: */    "bool",
                      /* params:          name            type       required */ 
                                        {{"domain_name",  "string",  true},
                                         {"price",        "asset",   true}},
                    /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(auction_domain_metadata);

    method_data transfer_domain_metadata{"transfer_domain", JSON_METHOD_IMPL(transfer_domain),
                       /* description */ "TODO: describe me",
                       /* returns: */    "bool",
                       /* params:          name            type       required */ 
                                         {{"domain_name",  "string",  true},
                                          {"to_address",   "address", true}},
                     /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(transfer_domain_metadata);

    method_data update_domain_record_metadata{"update_domain_record", JSON_METHOD_IMPL(update_domain_record),
                            /* description */ "TODO: describe me",
                            /* returns: */    "bool",
                            /* params:          name            type       required */ 
                                              {{"domain_name",  "string",  true},
                                               {"path",         "string",  true}},
                          /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(update_domain_record_metadata);

    method_data list_active_auctions_metadata{"list_active_auctions", JSON_METHOD_IMPL(list_active_auctions),
                            /* description */ "TODO: describe me",
                            /* returns: */    "vector<pair<asset,claim_domain_output>>",
                            /* params:     */ {},
                          /* prerequisites */ json_authenticated};
    register_method(list_active_auctions_metadata);

    method_data lookup_domain_record_metadata{"lookup_domain_record", JSON_METHOD_IMPL(lookup_domain_record),
                            /* description */ "TODO: describe me",
                            /* returns: */    "string",
                            /* params:          name            type       required */ 
                                              {{"domain_name",  "string",  true}},
                          /* prerequisites */ json_authenticated};
    register_method(lookup_domain_record_metadata);
#undef JSON_METHOD_IMPL
  }

  dns_rpc_server::~dns_rpc_server()
  {
  }

} } // bts::dns