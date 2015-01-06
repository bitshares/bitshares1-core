#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

    share_type asset_record::available_shares()const
    {
        return maximum_share_supply - current_share_supply;
    }

    bool asset_record::can_issue( const asset& amount )const
    {
        if( id != amount.asset_id ) return false;
        return can_issue( amount.amount );
    }

    bool asset_record::can_issue( const share_type amount )const
    {
        if( amount <= 0 ) return false;
        auto new_share_supply = current_share_supply + amount;
        // catch overflow conditions
        return (new_share_supply > current_share_supply) && (new_share_supply <= maximum_share_supply);
    }

    const asset_db_interface& asset_record::db_interface( const chain_interface& db )
    { try {
        return db._asset_db_interface;
    } FC_CAPTURE_AND_RETHROW() }

    oasset_record asset_db_interface::lookup( const asset_id_type id )const
    { try {
        return lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    oasset_record asset_db_interface::lookup( const string& symbol )const
    { try {
        return lookup_by_symbol( symbol );
    } FC_CAPTURE_AND_RETHROW( (symbol) ) }

    void asset_db_interface::store( const asset_record& record )const
    { try {
        const oasset_record prev_record = lookup( record.id );
        if( prev_record.valid() )
        {
            if( prev_record->symbol != record.symbol )
                erase_from_symbol_map( prev_record->symbol );
        }

        insert_into_id_map( record.id, record );
        insert_into_symbol_map( record.symbol, record.id );
    } FC_CAPTURE_AND_RETHROW( (record) ) }

    void asset_db_interface::remove( const asset_id_type id )const
    { try {
        const oasset_record prev_record = lookup( id );
        if( prev_record.valid() )
        {
            erase_from_id_map( id );
            erase_from_symbol_map( prev_record->symbol );
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
