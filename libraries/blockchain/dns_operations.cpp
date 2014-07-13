#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/dns_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/dns_utils.hpp>
#include <bts/blockchain/dns_config.hpp>

#include <bts/blockchain/extended_address.hpp> // TITAN
#include <fc/crypto/aes.hpp>

#include <iostream>

namespace bts { namespace blockchain { 

    void domain_bid_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        FC_ASSERT( is_valid_domain( this->domain_name ) );
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        /* If record is invalid, nobody has bid on it yet. You can bid if you exceed current minimum bid. */
        if ( ! odomain_rec.valid() || odomain_rec->get_true_state() == domain_record::unclaimed)
        {
            FC_ASSERT( this->bid_amount >= P2P_MIN_INITIAL_BID, "Initial bid does not exceed min required bid");
            eval_state.required_fees += asset(this->bid_amount, 0);
            auto rec = domain_record();
            rec.domain_name = this->domain_name;
            rec.owner = this->bidder_address;
            rec.value = fc::variant("");
            rec.last_update = eval_state._current_state->now().sec_since_epoch();
            rec.state = domain_record::in_auction;
            rec.price = this->bid_amount;
            rec.next_required_bid = P2P_NEXT_REQ_BID(0, this->bid_amount);
            eval_state._current_state->store_domain_record( rec );
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
                auto to_last_bidder = odomain_rec->price * (1 - P2P_PENALTY_RATE);
                auto to_fees = this->bid_amount - to_last_bidder;
                eval_state.required_fees += asset(to_fees, 0);

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

                FC_ASSERT(paid_to_previous_bidder >= to_last_bidder, "Did not pay back enough to previous bidder");

                odomain_rec->owner = this->bidder_address;
                odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
                odomain_rec->price = this->bid_amount;
                odomain_rec->next_required_bid = P2P_NEXT_REQ_BID( odomain_rec->next_required_bid, this->bid_amount );
                odomain_rec->time_in_top = 0;
            }
            eval_state._current_state->store_domain_record( *odomain_rec );
        }
    }


    void domain_sell_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        FC_ASSERT( odomain_rec.valid(), "Trying to sell domain which does not exist" );
        FC_ASSERT( odomain_rec->get_true_state() == domain_record::owned
                || odomain_rec->get_true_state() == domain_record::in_sale,
                   "Attempting to sell a domain which is not in 'owned' or 'in_sale' state");
        FC_ASSERT( eval_state.check_signature( odomain_rec->owner ), "Sale not signed by owner" );
        FC_ASSERT( this->price > 0, "Price must be positive" );
        odomain_rec->state = domain_record::in_sale;
        odomain_rec->price = this->price;
        odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
        eval_state._current_state->store_domain_record( *odomain_rec );
    }

    void domain_cancel_sell_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        FC_ASSERT( odomain_rec.valid(), "Trying to cancel sale for domain which does not exist" );
        FC_ASSERT( odomain_rec->get_true_state() == domain_record::in_sale,
                   "Attempting to cancel sale for a domain which is not in 'in_sale' state");
        FC_ASSERT( eval_state.check_signature( odomain_rec->owner ), "Cancel sale not signed by owner" );
        odomain_rec->state = domain_record::owned;
        odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
        eval_state._current_state->store_domain_record( *odomain_rec );
    }

    void domain_buy_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        FC_ASSERT( odomain_rec.valid(), "Trying to buy a domain which does not exist" );
        FC_ASSERT( odomain_rec->get_true_state() == domain_record::in_sale,
                   "Attempting to buy a domain which is not in 'in_sale' state");
        share_type paid_to_owner = 0;
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
                        paid_to_owner += deposit.amount;
                    }
                }
            }
        }
        FC_ASSERT( paid_to_owner >= odomain_rec->price, "Did not pay enough to previous owner" );
        odomain_rec->state = domain_record::owned;
        odomain_rec->owner = this->new_owner;
        odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
        eval_state._current_state->store_domain_record( *odomain_rec );
    }

    void domain_transfer_operation::titan_transfer( const fc::ecc::private_key& one_time_private_key,
                                                    const fc::ecc::public_key& to_public_key,
                                                    const fc::ecc::private_key& from_private_key,
                                                    const std::string& memo_message,
                                                    const fc::ecc::public_key& memo_pub_key,
                                                    memo_flags_enum memo_type )
    {
        memo = titan_memo();
        auto secret = one_time_private_key.get_shared_secret( to_public_key );
        auto ext_to_public_key = extended_public_key(to_public_key);
        auto secret_ext_public_key = ext_to_public_key.child( fc::sha256::hash(secret) );
        auto secret_public_key = secret_ext_public_key.get_pub_key();
        owner = address( secret_public_key );

        auto check_secret = from_private_key.get_shared_secret( secret_public_key );

        memo_data memo_content;
        memo_content.set_message( memo_message );
        memo_content.from = memo_pub_key;
        memo_content.from_signature = check_secret._hash[0];
        memo_content.memo_flags = memo_type;

        memo->one_time_key = one_time_private_key.get_public_key();

        FC_ASSERT( memo.valid() );
        memo->encrypted_memo_data = fc::aes_encrypt( secret, fc::raw::pack( memo_content ) );
    }


    void domain_transfer_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        FC_ASSERT( odomain_rec.valid(), "Trying to update domain which does not exist" );
        FC_ASSERT( odomain_rec->get_true_state() == domain_record::owned,
                   "Attempting to update a domain which is not in 'owned' state");
        FC_ASSERT( eval_state.check_signature( odomain_rec->owner ), "Update not signed by owner" );
        odomain_rec->owner = this->owner;
        odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
        eval_state._current_state->store_domain_record( *odomain_rec );
    }

    void domain_update_info_operation::evaluate( transaction_evaluation_state& eval_state )
    {
        auto odomain_rec = eval_state._current_state->get_domain_record( this->domain_name );
        FC_ASSERT( odomain_rec.valid(), "Trying to update domain which does not exist" );
        FC_ASSERT( odomain_rec->get_true_state() == domain_record::owned,
                   "Attempting to update a domain which is not in 'owned' state");
        FC_ASSERT( is_valid_value( this->value ), "Trying to update with invalid value" );
        FC_ASSERT( eval_state.check_signature( odomain_rec->owner ), "Update not signed by owner" );
        odomain_rec->value = this->value;
        odomain_rec->last_update = eval_state._current_state->now().sec_since_epoch();
        eval_state._current_state->store_domain_record( *odomain_rec );
    }


}} // bts::blockchain
