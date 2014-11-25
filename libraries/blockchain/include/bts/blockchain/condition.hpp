#pragma once

namespace bts { namespace blockchain {

    struct condition
    {
    };

    struct multisig_condition : condition
    {
        uint32_t                required;
        std::set<address>       owners;
    };

    // pow
    // delegate_fraud
    // timelock
    // BTC spv utxo claim

}} // bts::blockchain

FC_REFLECT( bts::blockchain::condition, BOOST_PP_SEQ_NIL );
FC_REFLECT( bts::blockchain::multisig_condition, (required)(owners) );
