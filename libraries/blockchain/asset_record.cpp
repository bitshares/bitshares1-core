#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    bool asset_record::address_is_approved( const chain_interface& db, const address& addr )const
    { try {
        if( !flag_is_active( restricted_accounts ) )
            return true;

        if( authority.owners.count( addr ) > 0 )
            return true;

        const oaccount_record account_record = db.get_account_record( addr );
        if( account_record.valid() && !account_record->is_retracted() )
        {
            if( account_record->id == issuer_id || whitelist.count( account_record->id ) > 0 )
            {
                if( account_record->owner_address() == addr )
                    return true;

                if( account_record->active_address() == addr )
                    return true;
            }
        }

        return false;
    } FC_CAPTURE_AND_RETHROW( (addr) ) }

    bool asset_record::authority_is_retracting( const transaction_evaluation_state& eval_state )const
    { try {
        return flag_is_active( retractable_balances ) && eval_state.verify_authority( authority );
    } FC_CAPTURE_AND_RETHROW() }

    uint64_t asset_record::share_string_to_precision_unsafe( const string& share_string_with_trailing_decimals )
    { try {
        // Don't bother checking input

        string precision_string;
        const auto decimal_pos = share_string_with_trailing_decimals.find( '.' );
        if( decimal_pos != string::npos )
        {
            const string rhs = share_string_with_trailing_decimals.substr( decimal_pos + 1 );
            while( precision_string.size() < rhs.size() )
                precision_string += '0';
        }

        return std::stoull( '1' + precision_string );
    } FC_CAPTURE_AND_RETHROW( (share_string_with_trailing_decimals) ) }

    share_type asset_record::share_string_to_satoshi( const string& share_string, const uint64_t precision )
    { try {
        bool negative_found = false;
        bool decimal_found = false;
        for( const char c : share_string )
        {
            if( isdigit( c ) )
                continue;

            if( c == '-' && !negative_found )
            {
                negative_found = true;
                continue;
            }

            if( c == '.' && !decimal_found )
            {
                decimal_found = true;
                continue;
            }

            FC_CAPTURE_AND_THROW( invalid_asset_amount, (share_string) );
        }

        if( !is_power_of_ten( precision ) )
            FC_CAPTURE_AND_THROW( invalid_precision, (precision) );

        share_type satoshis = 0;

        const auto decimal_pos = share_string.find( '.' );
        const string lhs = share_string.substr( negative_found, decimal_pos );
        if( !lhs.empty() )
            satoshis += std::stoll( lhs ) * precision;

        if( decimal_found )
        {
            const size_t max_rhs_size = std::to_string( precision ).substr( 1 ).size();

            string rhs = share_string.substr( decimal_pos + 1 );
            if( rhs.size() > max_rhs_size )
                FC_CAPTURE_AND_THROW( invalid_asset_amount, (share_string)(max_rhs_size) );

            while( rhs.size() < max_rhs_size )
                rhs += '0';

            if( !rhs.empty() )
                satoshis += std::stoll( rhs );
        }

        if( satoshis > BTS_BLOCKCHAIN_MAX_SHARES )
            FC_CAPTURE_AND_THROW( amount_too_large, (satoshis)(BTS_BLOCKCHAIN_MAX_SHARES) );

        if( negative_found )
            satoshis *= -1;

        return satoshis;
    } FC_CAPTURE_AND_RETHROW( (share_string)(precision) ) }

    asset asset_record::asset_from_string( const string& amount )const
    { try {
        return asset( asset_record::share_string_to_satoshi( amount, precision ), id );
    } FC_CAPTURE_AND_RETHROW( (amount) ) }

    string asset_record::amount_to_string( share_type amount, bool append_symbol )const
    { try {
       const share_type shares = ( amount >= 0 ) ? amount : -amount;
       string decimal = fc::to_string( precision + ( shares % precision ) );
       decimal[0] = '.';
       auto str = fc::to_pretty_string( shares / precision ) + decimal;
       if( append_symbol ) str += " " + symbol;
       if( amount < 0 ) return "-" + str;
       return str;
    } FC_CAPTURE_AND_RETHROW( (amount)(append_symbol) ) }

    void asset_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( id >= 0 );
        FC_ASSERT( !symbol.empty() );
        FC_ASSERT( id == 0 || issuer_id == market_issuer_id || db.lookup<account_record>( issuer_id ).valid() );
        FC_ASSERT( !is_user_issued() || (authority.required > 0 && !authority.owners.empty()) );
        FC_ASSERT( !name.empty() );
        FC_ASSERT( is_power_of_ten( precision ) );
        FC_ASSERT( max_supply >= 0 && max_supply <= BTS_BLOCKCHAIN_MAX_SHARES );
        FC_ASSERT( withdrawal_fee >= 0 && withdrawal_fee <= max_supply );
        FC_ASSERT( market_fee_rate <= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE );
        FC_ASSERT( collected_fees >= 0 && collected_fees <= current_supply );
        FC_ASSERT( current_supply >= 0 && current_supply <= max_supply );
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    oasset_record asset_record::lookup( const chain_interface& db, const asset_id_type id )
    { try {
        return db.asset_lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    oasset_record asset_record::lookup( const chain_interface& db, const string& symbol )
    { try {
        return db.asset_lookup_by_symbol( symbol );
    } FC_CAPTURE_AND_RETHROW( (symbol) ) }

    void asset_record::store( chain_interface& db, const asset_id_type id, const asset_record& record )
    { try {
        const oasset_record prev_record = db.lookup<asset_record>( id );
        if( prev_record.valid() )
        {
            if( prev_record->symbol != record.symbol )
                db.asset_erase_from_symbol_map( prev_record->symbol );
        }

        db.asset_insert_into_id_map( id, record );
        db.asset_insert_into_symbol_map( record.symbol, id );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void asset_record::remove( chain_interface& db, const asset_id_type id )
    { try {
        const oasset_record prev_record = db.lookup<asset_record>( id );
        if( prev_record.valid() )
        {
            db.asset_erase_from_id_map( id );
            db.asset_erase_from_symbol_map( prev_record->symbol );
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
