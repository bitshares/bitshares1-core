#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/types.hpp>

#include <bts/blockchain/balance_record.hpp> // how else do I get FC_ASSERT ?

namespace bts { namespace blockchain {

    set<address> object_record::get_owners()
    {
        FC_ASSERT(!"unimplemented");
    }

}} // bts::blockchain
