#pragma once

#include <bts/dns/util.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;
using namespace bts::wallet;

class dns_wallet : public bts::wallet::wallet
{
    public:
        dns_wallet();
        ~dns_wallet();

        signed_transaction bid_on_domain(const std::string &name, const asset &bid_price,
                                         const signed_transactions &tx_pool, dns_db &db);

        signed_transaction update_domain_record(const std::string &name, const fc::variant &value,
                                                const signed_transactions &tx_pool, dns_db &db);

        signed_transaction auction_domain(const std::string &name, const asset &ask_price,
                                          const signed_transactions &tx_pool, dns_db &db);

        signed_transaction collect_inputs_and_sign(signed_transaction &trx, const asset &min_amnt,
                                                   std::unordered_set<address> &req_sigs, const address &change_addr);

        signed_transaction collect_inputs_and_sign(signed_transaction &trx, const asset &min_amnt,
                                                   std::unordered_set<address> &req_sigs);

    protected:
        virtual bool scan_output(transaction_state &state, const trx_output &out, const output_reference &ref,
                                 const output_index &idx);

    private:
        signed_transaction update_or_auction_domain(bool update, claim_domain_output &domain_output, asset amount,
                                                    const signed_transactions &tx_pool, dns_db &db);
};

typedef std::shared_ptr<dns_wallet> dns_wallet_ptr;

} } // bts::dns
