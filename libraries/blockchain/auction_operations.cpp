#include <bts/blockchain/auction_operations.hpp>
#include <bts/blockchain/auction_records.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

    // Always a user auction
    void auction_start_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        auto chain = eval_state._current_state;
        FC_ASSERT( eval_state.check_update_permission( this->item_id ), "You don't have permission to update that item" );

        user_auction_record auction;
        auction.item = this->item_id;
        auction.expiration = this->expiration;  // TODO
        auction.buy_now = this->buy_now_price;
        auction.user_fee = 0; // TODO

        auction.previous_bidder = 0;
        auction.previous_bid = this->min_price;
        auction.previous_bid_time = chain->now();
        auction.bid_count = 0;
        auction.bid_total = asset(0, this->min_price.asset_id);

        auction.balance_claimed = false;
        auction.object_claimed = false;

        auction._id = chain->new_object_id( obj_type::user_auction_object );
        chain->store_object_record( object_record( auction ) );
    }

    void evaluate_throttled_auction_bid( transaction_evaluation_state& eval_state, const auction_bid_operation& op)
    {
        // Check that the auction is active
        // Pay up
        // Update auction state
    }

    void evaluate_user_auction_bid( transaction_evaluation_state& eval_state, const auction_bid_operation& op)
    {
         // Check that the auction is active
         // Pay up
         // Update auction state
    }

    void auction_bid_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        auto object = eval_state._current_state->get_object_record( this->auction_id );
        FC_ASSERT( object.valid(), "No such auction.");
        if( object->type() == obj_type::user_auction_object )
            evaluate_user_auction_bid( eval_state, *this );
        else if( object->type() == obj_type::throttled_auction_object )
            evaluate_throttled_auction_bid( eval_state, *this );
        else
            FC_ASSERT( false, "That is not an auction type!" );
    }

    void user_auction_claim_operation::evaluate( transaction_evaluation_state& eval_state )const
    { try {
        auto chain = eval_state._current_state;
        auto obj = chain->get_object_record( this->auction_id );
        FC_ASSERT( obj.valid(), "No such auction." );
        auto auction = obj->as<user_auction_record>();
        FC_ASSERT( auction.is_complete( *chain ), "Auction is not over yet." );
        if( this->claim_balance )
        {
            FC_ASSERT( NOT auction.balance_claimed,
                    "Those auction winnings have already been claimed." );
            eval_state.check_update_permission( auction.beneficiary );
            eval_state.add_balance( auction.previous_bid );
            auction.balance_claimed = true;
        }
        if( this->claim_object ) // set the item's owner to the winner
        {
            FC_ASSERT( NOT auction.object_claimed, "That object has already been claimed." );
            eval_state.check_update_permission( auction.previous_bidder );
            auto item = chain->get_object_record( auction.item );
            FC_ASSERT( item->owner_object == auction._id, "An auction was being held for an item owned by someone else?");
            item->owner_object = auction.previous_bidder;
            chain->store_object_record( *item );
            auction.object_claimed = true;
        }
        chain->store_object_record( object_record( auction ) );
    } FC_CAPTURE_AND_RETHROW( (eval_state)(*this) ) }


}} //bts::blockchain
