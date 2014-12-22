#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

    // The difference between a condition and normal business logic is that
    // condition proof equivalence classes always yield the same condition
    // and so you can embed the proof at the right "scope" in the transaction
    // without doing anything complicated.    
    // eval(with[cond](expr)) would mean:   push_permission prove("with[cond](expr)"); eval(expr)

    struct condition
    {
    };
    struct proof
    {
        condition get_condition();
    };

    struct multisig_condition : condition
    {
        uint32_t                 required; // 0 means anyone can claim. -1 means the DAC can
        std::set<address>        owners;

        multisig_condition():required(-1),owners(set<address>()){}
        multisig_condition( uint32_t m, set<address> owners )
            :required(m),owners(owners)
        {}
    };

    // pow
    // delegate_fraud
    // timelock
    // BTC spv utxo claim
    // cross-chain trading
    // 

}} // bts::blockchain

FC_REFLECT( bts::blockchain::condition, BOOST_PP_SEQ_NIL );
FC_REFLECT( bts::blockchain::multisig_condition, (required)(owners) );
