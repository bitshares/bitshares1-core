#include <bts/blockchain/site_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    void site_create_operation::evaluate( transaction_evaluation_state& eval_state ) 
    {
        FC_ASSERT( !"This operation is not enabled yet!" );

        // Create a site whose owner is the DAC.
        // Create a new auction for the site.
        FC_ASSERT(!"unimplemented");
    }

    void site_update_operation::evaluate( transaction_evaluation_state& eval_state ) 
    {
        FC_ASSERT( !"This operation is not enabled yet!" );

        // Check the owner of the site.
        FC_ASSERT(!"unimplemented");
    }


}} //bts::blockchain
