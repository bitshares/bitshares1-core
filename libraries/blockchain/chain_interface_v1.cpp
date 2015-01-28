#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <locale>

#define ASSET_MAX_SYMBOL_SIZE 8
#define ASSET_MIN_SYMBOL_SIZE 3

#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE    (BTS_BLOCKCHAIN_BLOCKS_PER_DAY / 12)
#define BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE       (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14)

namespace bts { namespace blockchain {

bool chain_interface::is_valid_symbol_name_v1( const string& symbol )const
{ try {
    FC_ASSERT( symbol != "BTSX" );

    if( symbol.size() < ASSET_MIN_SYMBOL_SIZE || symbol.size() > ASSET_MAX_SYMBOL_SIZE )
        return false;

    int dots = 0;
    for( const char c : symbol )
    {
        if( c == '.' )
        {
           if( ++dots > 1 )
            return false;
        }
        else if( !std::isalnum( c, std::locale::classic() ) || !std::isupper( c, std::locale::classic() ) )
            return false;
    }
    if( symbol.back() == '.' ) return false;
    if( symbol.front() == '.' ) return false;

    return true;
} FC_CAPTURE_AND_RETHROW( (symbol) ) }

share_type chain_interface::get_delegate_registration_fee_v1( uint8_t pay_rate )const
{
   if( pay_rate == 0 ) return 0;
   const share_type unscaled_fee = (get_delegate_pay_rate_v1() * BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE)/BTS_BLOCKCHAIN_NUM_DELEGATES;
   return (unscaled_fee * pay_rate) / 100;
}

share_type chain_interface::get_asset_registration_fee_v1()const
{
   return (get_delegate_pay_rate_v1() * BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE);
}

share_type chain_interface::get_delegate_pay_rate_v1()const
{
    const auto base_record = get_asset_record( asset_id_type( 0 ) );
    FC_ASSERT( base_record.valid() );
    return base_record->collected_fees / (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14);
}

bool chain_interface::is_fraudulent_asset( const asset_record& suspect_record )const
{ try {
    if( !suspect_record.is_user_issued() ) return false;

    const string& symbol = suspect_record.symbol;
    if( symbol.size() <= 3 || symbol.find( "BIT" ) != 0 ) return false;

    const oasset_record victim_record = get_asset_record( symbol.substr( 3 ) );
    return victim_record.valid() && victim_record->is_market_issued();
} FC_CAPTURE_AND_RETHROW( (suspect_record) ) }

} } // bts::blockchain
