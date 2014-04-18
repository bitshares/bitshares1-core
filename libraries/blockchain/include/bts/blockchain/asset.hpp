#pragma once
#include <fc/uint128.hpp>
#include <fc/io/enum_type.hpp>
#include <stdint.h>

namespace bts { namespace blockchain {

  struct price;

  /**
   *  An asset is a fixed point number with
   *  64.64 bit precision.  This is used
   *  for accumalating dividends and 
   *  calculating prices on the built in exchange.
   */
  struct asset
  {
      typedef uint16_t unit_type;

      asset( uint64_t a = 0, unit_type u = 0)
      :amount(a),unit(u){}

      asset& operator += ( const asset& o );
      asset& operator -= ( const asset& o );
      asset  operator *  ( const fc::uint128_t& fix6464 )const;
      asset  operator *  ( uint64_t constant )const
      {
         return asset( amount * constant );
      }
      asset  operator /  ( uint64_t constant )const
      {
         asset tmp(*this);
         tmp.amount /= constant;
         return tmp;
      }

      operator std::string()const;
      uint64_t get_rounded_amount()const;
       
      uint64_t   amount;
      unit_type  unit;
  };
  typedef asset::unit_type asset_type;
  
  /**
   *  A price is the result of dividing 2 asset classes and has
   *  the fixed point format 64.64 and -1 equals infinite.
   */
  struct price
  {
      static const fc::uint128& one();
      static const fc::uint128& infinite();

      price() {}
      price( const fc::uint128_t& r, asset_type base, asset_type quote )
      :ratio(r),base_unit(base),quote_unit(quote){}

      price( const std::string& s );
      price( double a, asset::unit_type base, asset::unit_type quote );
      operator std::string()const;
      operator double()const;

      fc::uint128_t ratio; // 64.64

      uint16_t asset_pair()const { return (uint16_t(uint8_t(quote_unit))<<8) | uint8_t(base_unit); }

      asset_type base_unit;
      asset_type quote_unit;
  };

  inline bool operator == ( const asset& l, const asset& r ) { return l.amount == r.amount; }
  inline bool operator != ( const asset& l, const asset& r ) { return l.amount != r.amount; }
  inline bool operator <  ( const asset& l, const asset& r ) { return l.amount <  r.amount; }
  inline bool operator >  ( const asset& l, const asset& r ) { return l.amount >  r.amount; }
  inline bool operator <= ( const asset& l, const asset& r ) { return l.unit == r.unit && l.amount <= r.amount; }
  inline bool operator >= ( const asset& l, const asset& r ) { return l.unit == r.unit && l.amount >= r.amount; }
  inline asset operator +  ( const asset& l, const asset& r ) { return asset(l) += r; }
  inline asset operator -  ( const asset& l, const asset& r ) { return asset(l) -= r; }

  inline bool operator == ( const price& l, const price& r ) { return l.ratio == r.ratio; }
  inline bool operator != ( const price& l, const price& r ) { return l.ratio == r.ratio; }
  inline bool operator <  ( const price& l, const price& r ) { return l.ratio <  r.ratio; }
  inline bool operator >  ( const price& l, const price& r ) { return l.ratio >  r.ratio; }
  inline bool operator <= ( const price& l, const price& r ) { return l.ratio <= r.ratio && l.asset_pair() == r.asset_pair(); }
  inline bool operator >= ( const price& l, const price& r ) { return l.ratio >= r.ratio && l.asset_pair() == r.asset_pair(); }

  /**
   *  A price will reorder the asset types such that the
   *  asset type with the lower enum value is always the
   *  denominator.  Therefore  bts/usd and  usd/bts will
   *  always result in a price measured in usd/bts because
   *  bitasset_type::bit_shares <  bitasset_type::bit_usd.
   */
  price operator / ( const asset& a, const asset& b );

  /**
   *  Assuming a.type is either the numerator.type or denominator.type in
   *  the price equation, return the number of the other asset type that
   *  could be exchanged at price p.
   *
   *  ie:  p = 3 usd/bts & a = 4 bts then result = 12 usd
   *  ie:  p = 3 usd/bts & a = 4 usd then result = 1.333 bts 
   */
  asset operator * ( const asset& a, const price& p );



} } // bts::blockchain

namespace fc
{
 //  void to_variant( const bts::blockchain::asset& var,  variant& vo );
 //  void from_variant( const variant& var,  bts::blockchain::asset& vo );
 //  void to_variant( const bts::blockchain::price& var,  variant& vo );
 //  void from_variant( const variant& var,  bts::blockchain::price& vo );
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::price, (ratio)(quote_unit)(base_unit) );
FC_REFLECT( bts::blockchain::asset, (amount)(unit) );

