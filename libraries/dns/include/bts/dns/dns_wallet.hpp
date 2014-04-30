#pragma once

#include <bts/dns/dns_transaction_validator.hpp>
#include <bts/wallet/wallet.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;
using namespace bts::wallet;

class dns_wallet : public bts::wallet::wallet
{
    public:
        dns_wallet(const dns_db_ptr& db);
        ~dns_wallet();

        signed_transaction bid(const std::string& key, const asset& bid_price,
                               const signed_transactions& pending_txs);

        signed_transaction ask(const std::string& key, const asset& ask_price,
                               const signed_transactions& pending_txs);

        signed_transaction transfer(const std::string& key, const address& recipient,
                                    const signed_transactions& pending_txs);

        signed_transaction release(const std::string& key,
                                   const signed_transactions& pending_txs);

        signed_transaction set(const std::string& key, const fc::variant& value,
                               const signed_transactions& pending_txs);

        fc::variant lookup(const std::string& key,
                           const signed_transactions& pending_txs);

        std::vector<trx_output> get_active_auctions();

        virtual std::string get_input_info_string(chain_database& db, const trx_input& in) override;
        virtual std::string get_output_info_string(const trx_output& out) override;

    protected:
        virtual bool scan_output(transaction_state& state, const trx_output& out, const output_reference& ref,
                                 const output_index& idx) override;

    private:
        dns_db_ptr                    _db;
        dns_transaction_validator_ptr _transaction_validator;

        signed_transaction update(const claim_dns_output& dns_output, const asset& amount,
                                  const output_reference& prev_tx_ref, const address& prev_owner);

        std::vector<std::string> get_keys_from_txs(const signed_transactions& txs);
        std::vector<std::string> get_keys_from_unspent(const std::map<output_index, trx_output>& unspent_outputs);

        bool key_is_available(const std::string& key, const signed_transactions& pending_txs, bool& new_or_expired,
                              output_reference& prev_tx_ref);
        bool key_is_useable(const std::string& key, const signed_transactions& pending_txs, output_reference& prev_tx_ref);
};

typedef std::shared_ptr<dns_wallet> dns_wallet_ptr;

} } // bts::dns
