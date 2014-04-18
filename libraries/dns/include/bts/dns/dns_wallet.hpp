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

        // TODO: Do we need tx_pool for every function like this?
        signed_transaction bid_on_domain(const std::string &name, const asset &bid_price,
                                         const signed_transactions &tx_pool, dns_db &db);

        signed_transaction update_domain(const std::string &name, const fc::variant &value,
                                         const signed_transactions &tx_pool, dns_db &db);

        signed_transaction transfer_domain(const std::string &name, const address &recipient,
                                           const signed_transactions &tx_pool, dns_db &db);

        signed_transaction auction_domain(const std::string &name, const asset &ask_price,
                                          const signed_transactions &tx_pool, dns_db &db);
        
        virtual std::string get_output_info_string(const trx_output& out);
        virtual std::string get_input_info_string(bts::blockchain::chain_database& db, const trx_input& in);
    protected:
        virtual bool scan_output(transaction_state &state, const trx_output &out, const output_reference &ref,
                                 const output_index &idx);

    private:
        signed_transaction update_or_auction_domain(const claim_domain_output &domain_output, const asset &amount,
                                                    const output_reference &prev_tx_ref, const address &prev_owner,
                                                    dns_db &db);
};

typedef std::shared_ptr<dns_wallet> dns_wallet_ptr;

} } // bts::dns
