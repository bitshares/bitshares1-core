#include <bts/blockchain/asset_record.hpp>

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

bool asset_record::can_issue( const share_type& amount )const
{
    if( amount <= 0 ) return false;
    auto new_share_supply = current_share_supply + amount;
    // catch overflow conditions
    return (new_share_supply > current_share_supply) && (new_share_supply <= maximum_share_supply);
}

bool asset_record::is_null()const
{
    return issuer_account_id == -1;
}

/** the asset is issued by the market and not by any user */
bool asset_record::is_market_issued()const
{
   switch( issuer_account_id )
   {
      case market_issued_asset:
      case market_feed_issued_asset:
         return true;
      default:
         return false;
   }
}

bool asset_record::uses_market_feed()const
{
  return issuer_account_id == market_feed_issued_asset;
}

asset_record asset_record::make_null()const
{
    asset_record cpy(*this);
    cpy.issuer_account_id = -1;
    return cpy;
}

uint64_t asset_record::get_precision()const
{
    return precision ? precision : 1;
}

}} // bts::blockchain
