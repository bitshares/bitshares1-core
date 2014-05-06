#pragma once

#include <bts/blockchain/transaction_validator.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_outputs.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

class dns_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        dns_transaction_validator(dns_db* db);
        virtual ~dns_transaction_validator() override;

        virtual block_evaluation_state_ptr create_block_state() const override = 0;

        virtual transaction_summary evaluate(const signed_transaction& tx,
                                             const block_evaluation_state_ptr& block_state) override = 0;

        virtual void validate_input(const meta_trx_input& in, transaction_evaluation_state& state,
                                    const block_evaluation_state_ptr& block_state) override = 0;

        virtual void validate_output(const trx_output& out, transaction_evaluation_state& state,
                                     const block_evaluation_state_ptr& block_state) override = 0;

        virtual bool is_valid_output(const claim_dns_output& output) = 0;
        virtual bool is_valid_key(const std::string& key) = 0;
        virtual bool is_valid_value(const std::vector<char>& value) = 0;

        virtual bool is_valid_bid_price(const asset& bid_price, const asset& prev_bid_price) = 0;
        virtual asset get_bid_transfer_amount(const asset& bid_price, const asset& prev_bid_price) = 0;

        virtual bool auction_is_closed(const output_reference& output_ref) = 0;
        virtual bool key_is_expired(const output_reference& output_ref) = 0;

        virtual bool key_is_available(const std::string& key, const std::vector<std::string>& pending_keys,
                                      bool& new_or_expired, output_reference& prev_output_ref) = 0;
        virtual bool key_is_useable(const std::string& key, const std::vector<std::string>& pending_keys,
                                    const std::vector<std::string>& unspent_keys, output_reference& prev_output_ref) = 0;

    protected:
        dns_db* _dns_db;
};

typedef std::shared_ptr<dns_transaction_validator> dns_transaction_validator_ptr;

} } // bts::dns
