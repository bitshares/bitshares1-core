#pragma once

namespace bts { namespace blockchain {

    struct lsrm_balance
    {
        object_id_type         id;
        address                market_id;
        uint64_t               balance;
        uint64_t               outcome_id;
    };

    struct lsmr_record
    {
        object_id_type         id;
        vector<int>            dimensions
        vector<object_id_type> events  
        vector<uint64_t>       values;

        uint64_t               liquidity_parameter;
        uint8_t                precision;

        get_cost();
        get_price();
    };

}} // bts::blockchain
