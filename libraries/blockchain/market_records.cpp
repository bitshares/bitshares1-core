#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_records.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/variant.hpp>
#include <sstream>
#include <boost/algorithm/string.hpp>

namespace bts { namespace blockchain {

order_id_type market_order::get_id()const
{
    std::stringstream id_ss;
    id_ss << string( type )
          << string( market_index.order_price )
          << string( market_index.owner );
    return fc::ripemd160::hash( id_ss.str() );
}

string market_order::get_small_id()const
{
    string type_prefix = string( type );
    type_prefix = type_prefix.substr( 0, type_prefix.find( "_" ) );
    boost::to_upper( type_prefix );
    return type_prefix + "-" + string( get_id() ).substr( 0, 8 );
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
        asset_id = market_index.order_price.base_asset_id;
        break;
     case cover_order:
        asset_id = market_index.order_price.quote_asset_id;
        break;
     case null_order:
        FC_ASSERT( false, "Null Order" );
  }
  return asset( state.balance, asset_id );
}

price market_order::get_price( const price& relative )const
{ try {
   switch( order_type_enum(type) )
   {
      case short_order:
        if( relative == price() )
           return market_index.order_price;
        if( state.limit_price )
           return std::min( *state.limit_price, relative );
        return relative;
      case bid_order:
      case ask_order:
      case cover_order:
        return market_index.order_price;
      case null_order:
        FC_ASSERT( false, "Null Order" );
   }
   FC_ASSERT( false, "Should not reach this line" );
}
catch( const bts::blockchain::price_multiplication_undefined& )
{
    return price();
}
catch( const bts::blockchain::price_multiplication_overflow& )
{
    return price();
}
catch( const bts::blockchain::price_multiplication_underflow& )
{
    return price();
}
FC_CAPTURE_AND_RETHROW( (*this)(relative) ) }

price market_order::get_highest_cover_price()const
{ try {
  FC_ASSERT( type == cover_order );
  return asset( state.balance, market_index.order_price.quote_asset_id ) / asset( *collateral );
} FC_CAPTURE_AND_RETHROW() }

asset market_order::get_quantity( const price& relative )const
{
  switch( order_type_enum( type ) )
  {
     case bid_order:
     { // balance is in USD  divide by price
        return get_balance() * get_price(relative);
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
        return asset( (*collateral * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR) / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR );
     }
     default:
        FC_ASSERT( false, "Not Implemented" );
  }
  // NEVER GET HERE.....
  //return get_balance() * get_price();
}
asset market_order::get_quote_quantity( const price& relative )const
{
  switch( order_type_enum( type ) )
  {
     case bid_order:
     { // balance is in USD  divide by price
        return get_balance();
     }
     case ask_order:
     { // balance is in USD  divide by price
        return get_balance() * get_price(relative);
     }
     case short_order:
     {
        return get_balance() * get_price(relative);
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
