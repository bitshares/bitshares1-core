#include <bts/blockchain/market_records.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

string market_order::get_id()const
{
  return "ORDER-" + fc::variant( market_index.owner ).as_string().substr(3,8);
}

asset market_order::get_balance()const
{
  asset_id_type asset_id;
  switch( type )
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
     default:
        FC_ASSERT( !"Not Implemented" );
  }
  return asset( state.balance, asset_id );
}

price market_order::get_price()const
{
  return market_index.order_price;
}

asset market_order::get_quantity()const
{
  switch( type )
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
        return get_balance() * get_price();
     }
     default:
        FC_ASSERT( !"Not Implemented" );
  }
  return get_balance() * get_price();
}
asset market_order::get_quote_quantity()const
{
  switch( type )
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
        return get_balance(); 
     }
     default:
        FC_ASSERT( !"Not Implemented" );
  }
  return get_balance() * get_price();
}

} } // bts::blockchain
