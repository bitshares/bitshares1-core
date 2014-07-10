#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/dns_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/dns_utils.hpp>
#include <bts/blockchain/dns_config.hpp>

namespace bts { namespace blockchain { 

    void update_domain_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        FC_ASSERT( is_valid_domain( this->domain_name ) );
        auto now = eval_state._current_state->now().sec_since_epoch();
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        /* First bid case */
        if (this->update_type == domain_record::unclaimed)
        {
            FC_ASSERT(!"unimplemented");
/*
            FC_ASSERT( ! domain_in_auction( odomain_rec ), "Attempt to start auction for a domain already in an auction" );
            FC_ASSERT( ! domain_owned_by_owner( odomain_rec ), "Someone already owns that domain." );

            FC_ASSERT(this->bid_amount >= P2P_MIN_INITIAL_BID, "Not large enough initial bid.");
            FC_ASSERT(this->value.as_string() == variant("").as_string());

            eval_state.required_fees += asset(this->bid_amount);
            
            domain_record updated_record = domain_record(); 
            updated_record.domain_name = this->domain_name;
            updated_record.owner = this->owner;
            updated_record.value = this->value;
            updated_record.last_update = now;
            updated_record.update_type = domain_record::first_bid;
            updated_record.last_bid = this->bid_amount;
            updated_record.next_required_bid = P2P_NEXT_REQ_BID(0, this->bid_amount);
            
            eval_state._current_state->store_domain_record( updated_record );
*/
        }
        /* Normal bid case */
        else if( this->update_type == domain_record::in_auction )
        {
            FC_ASSERT(!"unimplemented");
            /* 
            FC_ASSERT( domain_in_auction( odomain_rec ),
                     "Attempting to make a normal bid on a domain that is not in auction.");
            FC_ASSERT(this->bid_amount >= odomain_rec->next_required_bid,
                     "Bid is not high enough.");

            auto bid_difference = this->bid_amount - odomain_rec->last_bid;
            share_type paid_to_previous_bidder = 0;
            for (auto op : eval_state.trx.operations)
            {
                if (op.type == operation_type_enum::deposit_op_type)
                {
                    auto deposit = op.as<deposit_operation>();
                    if (deposit.condition.type == withdraw_condition_types::withdraw_signature_type)
                    {
                        auto condition = deposit.condition.as<withdraw_with_signature>();
                        if (condition.owner == odomain_rec->owner)
                        {
                            paid_to_previous_bidder += deposit.amount;
                        }
                    }
                }
            }
            ilog("expecting to get paid back ${amt}", ("amt", odomain_rec->last_bid + (bid_difference * P2P_KICKBACK_RATIO) ) );
            ilog("actually got back: ${amt}", ("amt", paid_to_previous_bidder));

            FC_ASSERT(paid_to_previous_bidder == odomain_rec->last_bid + (bid_difference * P2P_KICKBACK_RATIO),
                     "Did not pay back enough to previous owner.");
            eval_state.required_fees += asset(bid_difference * P2P_DIVIDEND_RATIO);

            FC_ASSERT(this->value.as_string() == variant("").as_string());
            
            domain_record updated_record = domain_record(); 
            updated_record.domain_name = this->domain_name;
            updated_record.owner = this->owner;
            updated_record.value = this->value;
            updated_record.last_update = now;
            updated_record.update_type = domain_record::bid;
            updated_record.last_bid = this->bid_amount;
            updated_record.next_required_bid = P2P_NEXT_REQ_BID(odomain_rec->next_required_bid, this->bid_amount);
            
            eval_state._current_state->store_domain_record( updated_record ); 
            */
        }
        else if( this->update_type == domain_record::in_sale )
        {
            FC_ASSERT(!"unimplemented");
            /*
            FC_ASSERT( domain_owned_by_owner(odomain_rec), "This domain is not owned by anyone." );
            FC_ASSERT( eval_state.check_signature( odomain_rec->owner ) );
            
            domain_record updated_record = domain_record();
            updated_record.domain_name = this->domain_name;
            updated_record.owner = this->owner;
            updated_record.value = variant("");
            updated_record.last_update = now;
            updated_record.update_type = domain_record::sell;
            updated_record.last_bid = this->bid_amount;
            updated_record.next_required_bid = P2P_NEXT_REQ_BID(this->bid_amount, this->bid_amount);
            
            eval_state._current_state->store_domain_record( updated_record ); 
            */
        }
        else if( this->update_type == domain_record::owned )
        {
            FC_ASSERT(!"unimplemented");
            /*
            FC_ASSERT( domain_owned_by_owner(odomain_rec), "This domain is not owned by anyone.");
            FC_ASSERT( eval_state.check_signature( odomain_rec->owner ) );

            domain_record updated_record = domain_record();
            updated_record.domain_name = this->domain_name;
            updated_record.owner = this->owner;
            updated_record.value = this->value;
            updated_record.last_update = now;
            updated_record.update_type = domain_record::info;
            updated_record.last_bid = 0;
            updated_record.next_required_bid = 0;
 
            eval_state._current_state->store_domain_record( updated_record ); 
            */
        }
    }

}} // bts::blockchain
