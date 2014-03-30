#pragma once

#include <bts/blockchain/transaction_validator.hpp>
#include <bts/dns/outputs.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

class dns_db;

class dns_tx_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    public:
        dns_tx_evaluation_state(const signed_transaction& tx):transaction_evaluation_state(tx)
        {
            seen_domain_input = false;
            seen_domain_output = false; 
        }

        bool seen_domain_input; // only one domain input/output per tx
        bool seen_domain_output;

        trx_output prev_output; // the previous output, from the tx input
        claim_domain_output prev_dns_output; // the previous output, from the tx input
};

class dns_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        dns_transaction_validator(dns_db* db);
        virtual ~dns_transaction_validator();

        virtual transaction_summary evaluate( const signed_transaction& tx, 
                                              const block_evaluation_state_ptr& block_state );
        virtual void validate_input( const meta_trx_input& in, 
                                     transaction_evaluation_state& state,
                                     const block_evaluation_state_ptr& block_state);
        virtual void validate_output( const trx_output& out, 
                                     transaction_evaluation_state& state,
                                     const block_evaluation_state_ptr& block_state);
        void validate_domain_input(const meta_trx_input& in, transaction_evaluation_state& state, const block_evaluation_state_ptr& block_state);

        void validate_domain_output(const trx_output& out, transaction_evaluation_state& state, const block_evaluation_state_ptr& block_state);
};

}} // bts::dns

class dns_block_eval_state : public bts::blockchain::block_evaluation_state
{
    public:
        virtual ~dns_block_eval_state();
};

FC_REFLECT(bts::dns::dns_tx_evaluation_state,
        (seen_domain_input)(seen_domain_output)(prev_output)(prev_dns_output));
