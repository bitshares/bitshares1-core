#pragma once

#include <bts/blockchain/transaction_validator.hpp>
#include <bts/dns/outputs.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

class dns_db;

class dns_tx_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    public:
        dns_tx_evaluation_state(const signed_transaction &tx) : transaction_evaluation_state(tx)
        {
            seen_dns_input = false;
            seen_dns_output = false;
        }

        claim_dns_output dns_input;
        asset dns_input_amount;

        /* Only one dns input/output per tx */
        bool seen_dns_input;
        bool seen_dns_output;
};

class dns_block_evaluation_state : public bts::blockchain::block_evaluation_state
{
    public:
        std::vector<std::string> key_pool;
};

typedef std::shared_ptr<dns_block_evaluation_state> dns_block_evaluation_state_ptr;

class dns_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        dns_transaction_validator(dns_db *db);
        ~dns_transaction_validator();

        virtual block_evaluation_state_ptr create_block_state() const;

        virtual transaction_summary evaluate(const signed_transaction &tx,
                                             const block_evaluation_state_ptr &block_state);

        virtual void validate_input(const meta_trx_input &in, transaction_evaluation_state &state,
                                    const block_evaluation_state_ptr &block_state);

        virtual void validate_output(const trx_output &out, transaction_evaluation_state &state,
                                     const block_evaluation_state_ptr &block_state);

        void validate_dns_input(const claim_dns_output &input, const asset &amount,
                                   dns_tx_evaluation_state &state,
                                   const dns_block_evaluation_state_ptr &block_state);

        void validate_dns_output(const claim_dns_output &output, const asset &amount,
                                    dns_tx_evaluation_state &state,
                                    const dns_block_evaluation_state_ptr &block_state);

    protected:
        dns_db *_dns_db;
};

} } // bts::dns

FC_REFLECT(bts::dns::dns_tx_evaluation_state, (seen_dns_input)(seen_dns_output)(dns_input)(dns_input_amount));
