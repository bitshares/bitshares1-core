#pragma once
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/lotto/lotto_outputs.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace lotto {
using namespace bts::blockchain;

class lotto_db;
class lotto_trx_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    public:
        lotto_trx_evaluation_state( const signed_transaction& tx )
        :transaction_evaluation_state( tx ),total_ticket_sales(0){}

        uint64_t total_ticket_sales;
        uint64_t ticket_winnings;
};

class lotto_transaction_validator : public bts::blockchain::transaction_validator
{
    public:
        lotto_transaction_validator(lotto_db* db);
        virtual ~lotto_transaction_validator();

        virtual transaction_summary evaluate( const signed_transaction& trx );
        virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
        virtual void validate_output( const trx_output& out, transaction_evaluation_state& state );

        void validate_ticket_input(const meta_trx_input& in, transaction_evaluation_state& state);
        void validate_ticket_output(const trx_output& out, transaction_evaluation_state& state);
};

}} // bts::lotto

