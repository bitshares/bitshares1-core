#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    share_type asset_record::available_shares()const
    { try {
        return maximum_share_supply - current_share_supply;
    } FC_CAPTURE_AND_RETHROW() }

    bool asset_record::can_issue( const asset& amount )const
    { try {
        if( id != amount.asset_id ) return false;
        return can_issue( amount.amount );
    } FC_CAPTURE_AND_RETHROW( (amount) ) }

    bool asset_record::can_issue( const share_type amount )const
    { try {
        if( amount <= 0 ) return false;
        auto new_share_supply = current_share_supply + amount;
        // catch overflow conditions
        return (new_share_supply > current_share_supply) && (new_share_supply <= maximum_share_supply);
    } FC_CAPTURE_AND_RETHROW( (amount) ) }

    bool asset_record::is_authorized( const address& addr )const
    { try {
        if( !is_restricted() )
            return true;

        if( authority.owners.count( addr ) > 0 )
            return true;

        return whitelist.count( addr ) > 0;
    } FC_CAPTURE_AND_RETHROW( (addr) ) }

    asset asset_record::asset_from_string( const string& amount )const
    { try {
       if( amount.find( ',' ) != string::npos )
           FC_CAPTURE_AND_THROW( invalid_asset_amount, (amount) );

       asset ugly_asset(0, id);

       // Multiply by the precision and truncate if there are extra digits.
       // example: 100.500019 becomes 10050001
       const auto decimal = amount.find(".");
       ugly_asset.amount += atoll(amount.substr(0, decimal).c_str()) * precision;

       if( decimal != string::npos )
       {
          string fraction_string = amount.substr(decimal+1);
          share_type fraction = atoll(fraction_string.c_str());

          if( !fraction_string.empty() && fraction > 0 )
          {
             while( fraction < precision )
                fraction *= 10;
             while( fraction >= precision )
                fraction /= 10;
             while( fraction_string.size() && fraction_string[0] == '0')
             {
                fraction /= 10;
                fraction_string.erase(0, 1);
             }

             if( ugly_asset.amount >= 0 )
                ugly_asset.amount += fraction;
             else
                ugly_asset.amount -= fraction;
          }
       }

       return ugly_asset;
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
        FC_ASSERT( !name.empty() );
        FC_ASSERT( id == 0 || issuer_account_id == market_issuer_id || db.lookup<account_record>( issuer_account_id ).valid() );
        FC_ASSERT( is_power_of_ten( precision ) );
        FC_ASSERT( maximum_share_supply >= 0 && maximum_share_supply <= BTS_BLOCKCHAIN_MAX_SHARES );
        FC_ASSERT( current_share_supply >= 0 && current_share_supply <= maximum_share_supply );
        FC_ASSERT( collected_fees >= 0 && collected_fees <= current_share_supply );
        FC_ASSERT( transaction_fee >= 0 && transaction_fee <= maximum_share_supply );
        FC_ASSERT( market_fee <= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE );
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
