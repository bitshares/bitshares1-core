#include <bts/blockchain/auction_records.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/site_operations.hpp>
#include <bts/blockchain/site_record.hpp>

namespace bts { namespace blockchain {

    void site_create_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        auto chain = eval_state._current_state;

        auto existing_site = chain->lookup_site( this->site_name );
        FC_ASSERT( NOT existing_site.valid(), "That site name is already taken." );

        // Create a site whose owner is the DAC.
        // Create a new auction for the site.
        auto auction_id = chain->new_object_id( obj_type::throttled_auction_object );
        auto site_id = chain->new_object_id( obj_type::site_object );
        auto site = site_record( this->site_name );
        auto auction = throttled_auction_record( site_id );

        chain->store_object_record( object_record( site, site_object, site_id ) );
        chain->store_object_record( object_record( auction, throttled_auction_object, auction_id ) );
    }

    void site_update_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        auto chain = eval_state._current_state;
        // Check the owner of the site.
        eval_state.check_update_permission( this->site_id );
        auto obj = chain->get_object_record( this->site_id );
        auto existing_site = obj->as<site_record>();

        if( this->lease_payment.amount >= 0 )
        {
            FC_ASSERT( this->lease_payment.asset_id == existing_site.renewal_fee.asset_id,
                    "You are not paying the rewnewal fee with the correct asset type." );
            existing_site.expiration += (60*60*24*30) * (this->lease_payment.amount / existing_site.renewal_fee.amount);
        }
        obj->user_data = this->user_data;
        obj->owner_object = this->owner_id;
        obj->set_data( existing_site );
        chain->store_object_record( *obj );
    }


}} //bts::blockchain
