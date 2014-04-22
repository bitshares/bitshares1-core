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

      fc::variant bid_on_domain(fc::rpc::json_connection* json_connection, const fc::variants& params);
      fc::variant auction_domain(fc::rpc::json_connection* json_connection, const fc::variants& params);
      fc::variant transfer_domain(fc::rpc::json_connection* json_connection, const fc::variants& params);
      fc::variant update_domain_record(fc::rpc::json_connection* json_connection, const fc::variants& params);
      fc::variant list_active_auctions(fc::rpc::json_connection* json_connection, const fc::variants& params);
      fc::variant lookup_domain_record(fc::rpc::json_connection* json_connection, const fc::variants& params);
    };

    fc::variant dns_rpc_server_impl::bid_on_domain(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      _self->check_wallet_is_open();
      _self->check_wallet_unlocked();
      FC_ASSERT(params.size() == 2); // cmd name amount
      std::string name = params[0].as_string();
      asset bid = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->bid_on_domain(name, bid, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::auction_domain(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      _self->check_wallet_is_open();
      _self->check_wallet_unlocked();
      FC_ASSERT(params.size() == 2); // cmd name price
      std::string name = params[0].as_string();
      asset price = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->auction_domain(name, price, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::transfer_domain(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      _self->check_wallet_is_open();
      _self->check_wallet_unlocked();
      FC_ASSERT(params.size() == 2); // cmd name to
      std::string name = params[0].as_string();
      auto to_owner = params[1].as<bts::blockchain::address>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->transfer_domain(name, to_owner, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::update_domain_record(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      _self->check_wallet_is_open();
      _self->check_wallet_unlocked();
      FC_ASSERT(params.size() == 2); // cmd name path
      std::string name = params[0].as_string();
      asset bid = params[1].as<asset>();
      signed_transactions tx_pool;

      auto tx = get_dns_wallet()->bid_on_domain(name, bid, tx_pool, *get_dns_db());

      _self->get_client()->broadcast_transaction(tx);
      return fc::variant(true);
    }
    fc::variant dns_rpc_server_impl::list_active_auctions(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      FC_ASSERT(params.size() == 0); 
      std::vector<trx_output> active_auctions = get_active_auctions(*get_dns_db());

      std::vector<claim_domain_output> claim_domain_outputs;
      claim_domain_outputs.reserve(active_auctions.size());
      for (const trx_output& output : active_auctions)
      {
        claim_domain_outputs.push_back(to_domain_output(output));

        //std::cout << "[" << output.amount.get_rounded_amount() << "] " << dns_output.name << "\n";
      }
      return fc::variant(claim_domain_outputs);
    }
    fc::variant dns_rpc_server_impl::lookup_domain_record(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      _self->check_json_connection_authenticated(json_connection);
      FC_ASSERT(params.size() == 1); 
      std::string name = params[0].as_string();

      return lookup_value(name, *get_dns_db());
    }

  } // end namespace detail

  dns_rpc_server::dns_rpc_server() :
    my(new detail::dns_rpc_server_impl)
  {
    my->_self = this;

#define REGISTER_METHOD(METHODNAME, PARAMETER_DESCRIPTION, METHOD_DESCRIPTION) \
      register_method(#METHODNAME, boost::bind(&detail::dns_rpc_server_impl::METHODNAME, my.get(), _1, _2), PARAMETER_DESCRIPTION, METHOD_DESCRIPTION)

    REGISTER_METHOD(bid_on_domain, "<domain_name> <amount>", "");
    REGISTER_METHOD(auction_domain, "<domain_name> <price>", "");
    REGISTER_METHOD(transfer_domain, "<domain_name> <to_address>", "");
    REGISTER_METHOD(update_domain_record, "<domain_name> <path>", "");
    REGISTER_METHOD(list_active_auctions,"","");
    REGISTER_METHOD(lookup_domain_record,"<domain_name>","");

#undef REGISTER_METHOD
  }

  dns_rpc_server::~dns_rpc_server()
  {
  }

} } // bts::dns