#pragma once

#include <bts/dns/util.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;
using namespace bts::wallet;

class dns_wallet : public bts::wallet::wallet
{
    public:
        dns_wallet() {}
        ~dns_wallet() {}

        signed_transaction bid(const std::string& key, const asset& bid_price,
                               const signed_transactions& pending_txs, dns_db& db);

        signed_transaction ask(const std::string& key, const asset& ask_price,
                               const signed_transactions& pending_txs, dns_db& db);

        signed_transaction transfer(const std::string& key, const address& recipient,
                                    const signed_transactions& pending_txs, dns_db& db);

        signed_transaction release(const std::string& key,
                                   const signed_transactions& pending_txs, dns_db& db);

        signed_transaction set(const std::string& key, const fc::variant& value,
                               const signed_transactions& pending_txs, dns_db& db);

        fc::variant lookup(const std::string& key,
                           const signed_transactions& pending_txs, dns_db& db);

        virtual std::string get_input_info_string(bts::blockchain::chain_database& db, const trx_input& in);
        virtual std::string get_output_info_string(const trx_output& out);

        using bts::wallet::wallet::transfer;

    protected:
        virtual bool scan_output(transaction_state& state, const trx_output& out, const output_reference& ref,
                                 const output_index& idx);

        signed_transaction update(const claim_dns_output& dns_output, const asset& amount,
                                  const output_reference& prev_tx_ref, const address& prev_owner, dns_db& db);
};

typedef std::shared_ptr<dns_wallet> dns_wallet_ptr;

} } // bts::dns
