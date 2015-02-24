#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

    struct data_operation
    {
        static const operation_type_enum  type;
        uint64_t                          tag;
        std::vector<char>                 data;
        void evaluate( transaction_evaluation_state& eval_state )const;
    };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::data_operation,
            (tag)
            (data)
            )

