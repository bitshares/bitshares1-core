#include <bts/blockchain/market_records.hpp>
#include <fc/exception/exception.hpp>
#include <sstream>

namespace bts { namespace blockchain {

order_id_type market_order::get_id()const
{
    std::stringstream id_ss;
    id_ss << string( type )
          << string( market_index.order_price )
          << string( market_index.owner );
    return fc::ripemd160::hash( id_ss.str() );
}

asset market_order::get_balance()const
{
  asset_id_type asset_id;
  switch( order_type_enum( type ) )
  {
     case bid_order:
        asset_id = market_index.order_price.quote_asset_id;
        break;
     case ask_order:
        asset_id = market_index.order_price.base_asset_id;
        break;
     case short_order:
        asset_id = 0; // always base shares for shorts.
        break;
     case cover_order:
        asset_id = market_index.order_price.quote_asset_id; // always base shares for shorts.
        break;
     default:
        FC_ASSERT( false, "Not Implemented" );
  }
  return asset( state.balance, asset_id );
}

price market_order::get_price()const
{
  return market_index.order_price;
}
price market_order::get_highest_cover_price()const
{ try {
  FC_ASSERT( type == cover_order );
  return asset( state.balance, market_index.order_price.quote_asset_id ) / asset( *collateral );
} FC_CAPTURE_AND_RETHROW() }

asset market_order::get_quantity()const
{
  switch( order_type_enum( type ) )
  {
     case bid_order:
     { // balance is in USD  divide by price
        return get_balance() * get_price();
     }
     case ask_order:
     { // balance is in USD  divide by price
        return get_balance();
     }
     case short_order:
     {
        return get_balance();
     }
     case cover_order:
     {
        return asset( (*collateral * 3)/4 );
     }
     default:
        FC_ASSERT( false, "Not Implemented" );
  }
  // NEVER GET HERE.....
  //return get_balance() * get_price();
}
asset market_order::get_quote_quantity()const
{
  switch( order_type_enum( type ) )
  {
     case bid_order:
     { // balance is in USD  divide by price
        return get_balance();
     }
     case ask_order:
     { // balance is in USD  divide by price
        return get_balance() * get_price();
     }
     case short_order:
     {
        return get_balance() * get_price();
     }
     case cover_order:
     {
        return get_balance();
     }
     default:
        FC_ASSERT( false, "Not Implemented" );
  }
  // NEVER GET HERE.....
 // return get_balance() * get_price();
}

} } // bts::blockchain
