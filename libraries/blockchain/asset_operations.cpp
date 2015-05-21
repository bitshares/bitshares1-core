#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

bool is_power_of_ten( uint64_t n )
{
    switch( n )
    {
        case 1ll:
        case 10ll:
        case 100ll:
        case 1000ll:
        case 10000ll:
        case 100000ll:
        case 1000000ll:
        case 10000000ll:
        case 100000000ll:
        case 1000000000ll:
        case 10000000000ll:
        case 100000000000ll:
        case 1000000000000ll:
        case 10000000000000ll:
        case 100000000000000ll:
        case 1000000000000000ll:
            return true;
        default:
            return false;
    }
}

void create_asset_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( !eval_state.pending_state()->is_valid_symbol_name( this->symbol ) )
        FC_CAPTURE_AND_THROW( invalid_asset_symbol, (symbol) );

    const auto dot_pos = this->symbol.find('.');
    if( dot_pos != string::npos )
    {
        string parent_symbol = this->symbol.substr( 0, dot_pos );
        oasset_record parent_asset_record = eval_state.pending_state()->get_asset_record( parent_symbol );
        FC_ASSERT( parent_asset_record.valid() );

        if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_9_0_FORK_BLOCK_NUM )
        {
            FC_ASSERT( parent_asset_record->is_user_issued() );
            if( !eval_state.verify_authority( parent_asset_record->authority ) )
                FC_CAPTURE_AND_THROW( missing_signature, (parent_asset_record->authority) );
        }
        else
        {
            if( parent_asset_record->is_user_issued() )
            {
                if( !eval_state.verify_authority( parent_asset_record->authority ) )
                    FC_CAPTURE_AND_THROW( missing_signature, (parent_asset_record->authority) );
            }
        }
    }

    oasset_record current_asset_record = eval_state.pending_state()->get_asset_record( this->symbol );
    if( current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( asset_symbol_in_use, (symbol) );

    if( this->name.empty() )
        FC_CAPTURE_AND_THROW( invalid_asset_name, (this->name) );

    const asset_id_type asset_id = eval_state.pending_state()->last_asset_id() + 1;
    current_asset_record = eval_state.pending_state()->get_asset_record( asset_id );
    if( current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( asset_id_in_use, (asset_id) );

    oaccount_record issuer_account_record;
    if( issuer_account_id != asset_record::market_issuer_id )
    {
        issuer_account_record = eval_state.pending_state()->get_account_record( this->issuer_account_id );
        if( !issuer_account_record.valid() )
            FC_CAPTURE_AND_THROW( unknown_account_id, (issuer_account_id) );
    }

    if( this->maximum_share_supply <= 0 || this->maximum_share_supply > BTS_BLOCKCHAIN_MAX_SHARES )
        FC_CAPTURE_AND_THROW( invalid_asset_amount, (this->maximum_share_supply) );

    if( !is_power_of_ten( this->precision ) )
        FC_CAPTURE_AND_THROW( invalid_precision, (this->precision) );

    const asset reg_fee( eval_state.pending_state()->get_asset_registration_fee( this->symbol.size() ), 0 );
    eval_state.min_fees[ reg_fee.asset_id ] += reg_fee.amount;

    asset_record new_record;

    new_record.id                 = eval_state.pending_state()->new_asset_id();
    new_record.symbol             = this->symbol;

    new_record.issuer_id          = this->issuer_account_id;

    if( issuer_account_record.valid() )
    {
        new_record.authority.owners.insert( issuer_account_record->active_key() );
        new_record.authority.required = 1;
    }

    new_record.name               = this->name;
    new_record.description        = this->description;
    new_record.public_data        = this->public_data;

    new_record.precision          = this->precision;
    new_record.max_supply         = this->maximum_share_supply;

    new_record.registration_date  = eval_state.pending_state()->now();
    new_record.last_update        = new_record.registration_date;

    eval_state.pending_state()->store_asset_record( new_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void issue_asset_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    oasset_record current_asset_record = eval_state.pending_state()->get_asset_record( abs( this->amount.asset_id ) );
    if( !current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( unknown_asset_id, (amount.asset_id) );

    if( !current_asset_record->is_user_issued() )
        FC_CAPTURE_AND_THROW( not_user_issued, (*current_asset_record) );

    if( !eval_state.verify_authority( current_asset_record->authority ) )
        FC_CAPTURE_AND_THROW( missing_signature, (current_asset_record->authority) );

    if( this->amount.amount <= 0 )
        FC_CAPTURE_AND_THROW( negative_issue, (amount) );

    if( this->amount.asset_id > 0 )
    {
        if( this->amount.amount > current_asset_record->max_supply - current_asset_record->current_supply )
            FC_CAPTURE_AND_THROW( over_issue, (amount)(*current_asset_record) );

        current_asset_record->current_supply += this->amount.amount;
    }
    else
    {
        if( this->amount.amount > current_asset_record->collected_fees )
            FC_CAPTURE_AND_THROW( amount_too_large, (amount)(*current_asset_record) );

        current_asset_record->collected_fees -= this->amount.amount;
    }

    eval_state.add_balance( asset( this->amount.amount, abs( this->amount.asset_id ) ) );

    current_asset_record->last_update = eval_state.pending_state()->now();
    eval_state.pending_state()->store_asset_record( *current_asset_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void asset_update_properties_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    oasset_record current_asset_record = eval_state.pending_state()->get_asset_record( this->asset_id );
    if( !current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( unknown_asset_id, (this->asset_id) );

    if( !current_asset_record->is_user_issued() )
        FC_CAPTURE_AND_THROW( not_user_issued, (*current_asset_record) );

    if( !eval_state.verify_authority( current_asset_record->authority ) )
        FC_CAPTURE_AND_THROW( missing_signature, (current_asset_record->authority) );

    // Reject no-ops
    FC_ASSERT( this->issuer_id.valid()
               || this->name.valid()
               || this->description.valid()
               || this->public_data.valid()
               || this->precision.valid()
               || this->max_supply.valid()
               || this->withdrawal_fee.valid()
               || this->market_fee_rate.valid() );

    if( this->issuer_id.valid() )
    {
        const oaccount_record account_record = eval_state.pending_state()->get_account_record( *this->issuer_id );
        if( !account_record.valid() )
            FC_CAPTURE_AND_THROW( unknown_account_id, (*this->issuer_id) );

        current_asset_record->issuer_id = *this->issuer_id;
    }

    if( this->name.valid() )
    {
        if( this->name->empty() )
            FC_CAPTURE_AND_THROW( invalid_asset_name, (*this->name) );

        current_asset_record->name = *this->name;
    }

    if( this->description.valid() )
        current_asset_record->description = *this->description;

    if( this->public_data.valid() )
        current_asset_record->public_data = *this->public_data;

    if( this->precision.valid() )
    {
        if( current_asset_record->current_supply != 0 )
            FC_CAPTURE_AND_THROW( outstanding_shares_exist, (current_asset_record->current_supply) );

        if( !is_power_of_ten( *this->precision ) )
            FC_CAPTURE_AND_THROW( invalid_precision, (*this->precision) );

        current_asset_record->precision = *this->precision;
    }

    if( this->max_supply.valid() )
    {
        if( current_asset_record->current_supply != 0 && !current_asset_record->flag_is_active( asset_record::dynamic_max_supply ) )
            FC_CAPTURE_AND_THROW( outstanding_shares_exist, (current_asset_record->current_supply) );

        if( *this->max_supply < current_asset_record->current_supply || *this->max_supply > BTS_BLOCKCHAIN_MAX_SHARES )
            FC_CAPTURE_AND_THROW( invalid_asset_amount, (*this->max_supply) );

        current_asset_record->max_supply = *this->max_supply;
    }

    if( this->withdrawal_fee.valid() )
    {
        if( current_asset_record->current_supply != 0 && !current_asset_record->flag_is_active( asset_record::dynamic_fees ) )
            FC_CAPTURE_AND_THROW( outstanding_shares_exist, (current_asset_record->current_supply) );

        if( *this->withdrawal_fee < 0 || *this->withdrawal_fee > current_asset_record->max_supply )
            FC_CAPTURE_AND_THROW( invalid_asset_amount, (*this->withdrawal_fee) );

        current_asset_record->withdrawal_fee = *this->withdrawal_fee;
    }

    if( this->market_fee_rate.valid() )
    {
        if( current_asset_record->current_supply != 0 && !current_asset_record->flag_is_active( asset_record::dynamic_fees ) )
            FC_CAPTURE_AND_THROW( outstanding_shares_exist, (current_asset_record->current_supply) );

        if( *this->market_fee_rate > BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE )
            FC_CAPTURE_AND_THROW( invalid_fee_rate, (*this->market_fee_rate) );

        current_asset_record->market_fee_rate = *this->market_fee_rate;
    }

    current_asset_record->last_update = eval_state.pending_state()->now();
    eval_state.pending_state()->store_asset_record( *current_asset_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void asset_update_permissions_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    oasset_record current_asset_record = eval_state.pending_state()->get_asset_record( this->asset_id );
    if( !current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( unknown_asset_id, (this->asset_id) );

    if( !current_asset_record->is_user_issued() )
        FC_CAPTURE_AND_THROW( not_user_issued, (*current_asset_record) );

    if( !eval_state.verify_authority( current_asset_record->authority ) )
        FC_CAPTURE_AND_THROW( missing_signature, (current_asset_record->authority) );

    // Reject no-ops
    FC_ASSERT( this->authority.valid()
               || this->authority_flag_permissions.valid()
               || this->active_flags.valid() );

    if( this->authority.valid() )
    {
        if( this->authority->required < 1 || this->authority->owners.empty() )
            FC_CAPTURE_AND_THROW( invalid_authority, (*this->authority) );

        current_asset_record->authority = *this->authority;
    }

    if( this->authority_flag_permissions.valid() )
    {
        if( current_asset_record->current_supply != 0 )
        {
            for( size_t i = 0; i < sizeof( current_asset_record->authority_flag_permissions ) * 8; ++i )
            {
                const auto permission = static_cast<asset_record::flag_enum>( 1 << i );
                const bool requesting_permission = *this->authority_flag_permissions & permission;
                if( requesting_permission && !current_asset_record->authority_has_flag_permission( permission ) )
                    FC_CAPTURE_AND_THROW( outstanding_shares_exist, (permission) );
            }
        }

        current_asset_record->authority_flag_permissions = *this->authority_flag_permissions;
    }

    if( this->active_flags.valid() )
    {
        for( size_t i = 0; i < sizeof( current_asset_record->active_flags ) * 8; ++i )
        {
            const auto permission = static_cast<asset_record::flag_enum>( 1 << i );
            const bool activating_permission = *this->active_flags & permission;
            if( activating_permission && !current_asset_record->authority_has_flag_permission( permission ) )
                FC_CAPTURE_AND_THROW( permission_not_available, (permission) );
        }

        current_asset_record->active_flags = *this->active_flags;
    }

    current_asset_record->last_update = eval_state.pending_state()->now();
    eval_state.pending_state()->store_asset_record( *current_asset_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void asset_update_whitelist_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    oasset_record current_asset_record = eval_state.pending_state()->get_asset_record( this->asset_id );
    if( !current_asset_record.valid() )
        FC_CAPTURE_AND_THROW( unknown_asset_id );

    if( !current_asset_record->is_user_issued() )
        FC_CAPTURE_AND_THROW( not_user_issued, (*current_asset_record) );

    if( this->account_ids.empty() )
        FC_CAPTURE_AND_THROW( invalid_whitelist );

    for( const account_id_type id : this->account_ids )
    {
        const oaccount_record account_record = eval_state.pending_state()->get_account_record( abs( id ) );
        if( !account_record.valid() )
            FC_CAPTURE_AND_THROW( unknown_account_id );

        if( id > 0 )
            current_asset_record->whitelist.insert( id );
        else
            current_asset_record->whitelist.erase( abs( id ) );
    }

    eval_state.pending_state()->store_asset_record( *current_asset_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
