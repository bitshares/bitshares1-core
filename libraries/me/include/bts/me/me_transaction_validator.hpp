#pragma once
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/me/outputs.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace me {
using namespace bts::blockchain;

class me_db;

class me_tx_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    public:
        me_tx_evaluation_state( const signed_transaction& tx )
        :transaction_evaluation_state( tx ) {} 
};


class me_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        me_transaction_validator(me_db* db);
        virtual ~me_transaction_validator();

        virtual transaction_summary evaluate( const signed_transaction& tx );
        virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
        virtual void validate_output( const trx_output& out, transaction_evaluation_state& state );

        void validate_domain_input(const meta_trx_input& in, transaction_evaluation_state& state);
        void validate_domain_output(const trx_output& out, transaction_evaluation_state& state);
};

}} // bts::me

FC_REFLECT(bts::me::me_tx_evaluation_state, BOOST_PP_SEQ_NULL ) 
