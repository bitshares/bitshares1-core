#pragma once

#include <bts/blockchain/types.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace blockchain {

  struct price;

  /**
   *  An asset is a 64-bit amount of shares, and an
   *  asset_id specifying the type of shares.
   */
  struct asset
  {
      asset():amount(0),asset_id(0){}
      explicit asset( share_type a, asset_id_type u = 0)
      :amount(a),asset_id(u){}

      asset& operator += ( const asset& o );
      asset& operator -= ( const asset& o );
      asset  operator /  ( uint64_t constant )const
      {
         asset tmp(*this);
         tmp.amount /= constant;
         return tmp;
      }
      asset operator-()const { return asset( -amount, asset_id); }

      operator std::string()const;

      share_type     amount;
      asset_id_type  asset_id;
  };

  /**
   *  A price is the result of dividing 2 asset classes.  It is
   *  a 128-bit decimal fraction with denominator FC_REAL128_PRECISION
   *  together with units specifying the two asset ID's.
   *
   *  -1 is considered to be infinity.
   */
  struct price
  {
      static const fc::uint128& one();
      static const fc::uint128& infinite();

      price() {}
      price( const fc::uint128_t& r, const asset_id_type quote_id, const asset_id_type base_id )
      : ratio( r ), base_asset_id( base_id ), quote_asset_id( quote_id ) {}

      price( const string& s );
      int set_ratio_from_string( const string& ratio_str );
      string ratio_string()const;
      operator string()const;
      bool is_infinite() const;

      fc::uint128_t ratio; // This is a DECIMAL FRACTION with denominator equal to BTS_PRICE_PRECISION

      pair<asset_id_type,asset_id_type> asset_pair()const { return std::make_pair( quote_asset_id, base_asset_id ); }

      asset_id_type base_asset_id;
      asset_id_type quote_asset_id;
  };
  typedef optional<price> oprice;

  inline bool operator == ( const asset& l, const asset& r )
  {
      return std::tie( l.amount, l.asset_id ) == std::tie( r.amount, r.asset_id );
  }
  inline bool operator != ( const asset& l, const asset& r )
  {
      return !( l == r );
  }
  inline bool operator < ( const asset& l, const asset& r )
  {
      FC_ASSERT( l.asset_id == r.asset_id );
      return l.amount < r.amount;
  }
  inline bool operator > ( const asset& l, const asset& r )
  {
      FC_ASSERT( l.asset_id == r.asset_id );
      return l.amount > r.amount;
  }
  inline bool operator <= ( const asset& l, const asset& r )
  {
      return l < r || l == r;
  }
  inline bool operator >= ( const asset& l, const asset& r )
  {
      return l > r || l == r;
  }
  inline asset operator + ( const asset& l, const asset& r )
  {
      return asset( l ) += r;
  }
  inline asset operator - ( const asset& l, const asset& r )
  {
      return asset( l ) -= r;
  }

  inline bool operator == ( const price& l, const price& r )
  {
      return std::tie( l.ratio, l.base_asset_id, l.quote_asset_id ) == std::tie( r.ratio, r.base_asset_id, r.quote_asset_id );
  }
  inline bool operator != ( const price& l, const price& r )
  {
      return !( l == r );
  }

  price operator *  ( const price& l, const price& r );

  inline bool operator <  ( const price& l, const price& r )
  {
     if( l.quote_asset_id < r.quote_asset_id ) return true;
     if( l.quote_asset_id > r.quote_asset_id ) return false;
     if( l.base_asset_id < r.base_asset_id ) return true;
     if( l.base_asset_id > r.base_asset_id ) return false;
     return l.ratio <  r.ratio;
  }
  inline bool operator >  ( const price& l, const price& r )
  {
     if( l.quote_asset_id > r.quote_asset_id ) return true;
     if( l.quote_asset_id < r.quote_asset_id ) return false;
     if( l.base_asset_id > r.base_asset_id ) return true;
     if( l.base_asset_id < r.base_asset_id ) return false;
     return l.ratio >  r.ratio;
  }

  inline bool operator <= ( const price& l, const price& r ) { return l.ratio <= r.ratio && l.asset_pair() == r.asset_pair(); }
  inline bool operator >= ( const price& l, const price& r ) { return l.ratio >= r.ratio && l.asset_pair() == r.asset_pair(); }

  /**
   *  A price will reorder the asset types such that the
   *  asset type with the lower enum value is always the
   *  denominator.  Therefore  bts/usd and  usd/bts will
   *  always result in a price measured in usd/bts because
   *  bitasset_id_type::bit_shares < bitasset_id_type::bit_usd.
   */
  price operator / ( const asset& a, const asset& b );
  asset operator / ( const asset& a, const price& b );

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
   void to_variant( const bts::blockchain::price& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::price& vo );
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::asset, (amount)(asset_id) );
FC_REFLECT( bts::blockchain::price, (ratio)(quote_asset_id)(base_asset_id) );
