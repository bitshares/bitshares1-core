#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/dns_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/dns_utils.hpp>
#include <bts/blockchain/dns_config.hpp>

#include <iostream>

namespace bts { namespace blockchain { 

    void domain_bid_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        FC_ASSERT( is_valid_domain( this->domain_name ) );
        auto now = eval_state._current_state->now().sec_since_epoch();
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        /* If record is invalid, nobody has bid on it yet. You can bid if you exceed current minimum bid. */
        if ( ! odomain_rec.valid() )
        {
            FC_ASSERT( this->bid_amount >= P2P_MIN_INITIAL_BID, "Initial bid does not exceed min required bid");
            eval_state.required_fees += asset(this->bid_amount, 0);
            auto rec = domain_record();
            rec.domain_name = this->domain_name;
            rec.owner = this->bidder_address;
            rec.last_update = fc::time_point::now().sec_since_epoch();
            rec.state = domain_record::in_auction;
            rec.price = this->bid_amount;
            rec.next_required_bid = P2P_NEXT_REQ_BID(0, this->bid_amount);
            std::cerr << "domain is valid and I am storing it now\n";
            eval_state._current_state->store_domain_record( rec );
            std::cerr << "stored it!\n";
        }
        else
        {
            FC_ASSERT( odomain_rec->get_true_state() == domain_record::in_auction,
                       "Trying to bid on a domain that is not in auction" );
            if (false) // TODO if signed by owner
            {
            }
            else
            {
                FC_ASSERT( this->bid_amount >= odomain_rec->next_required_bid );
                eval_state.required_fees += asset(this->bid_amount, 0);
                odomain_rec->owner = this->bidder_address;
                odomain_rec->last_update = fc::time_point::now().sec_since_epoch();
                odomain_rec->price = this->bid_amount;
                odomain_rec->next_required_bid = P2P_NEXT_REQ_BID( odomain_rec->next_required_bid, this->bid_amount );
            }
            eval_state._current_state->store_domain_record( *odomain_rec );
        }
    }

}} // bts::blockchain
