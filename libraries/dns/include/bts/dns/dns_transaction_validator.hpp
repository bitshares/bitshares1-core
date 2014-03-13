#pragma once
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/dns/outputs.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace dns {
using namespace bts::blockchain;

class dns_tx_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    public:
        dns_tx_evaluation_state( const signed_transaction& tx )
        :transaction_evaluation_state( tx ) {}
        bool seen_domain_input; // only one domain input/output per tx
        bool seen_domain_output;
        trx_output claimed; // the previous output, from the tx input
        claim_domain_output dns_claimed; // the previous output, from the tx input
};


class dns_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        virtual transaction_summary evaluate( const signed_transaction& tx );
        virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
        virtual void validate_output( const trx_output& out, transaction_evaluation_state& state );
        void validate_domain_input(const meta_trx_input& in, transaction_evaluation_state& state);

        void validate_domain_output(const trx_output& out, transaction_evaluation_state& state);
};

}} // bts::dns

FC_REFLECT(bts::dns::dns_tx_evaluation_state, (seen_domain_input)(seen_domain_output)
                                              (claimed));
