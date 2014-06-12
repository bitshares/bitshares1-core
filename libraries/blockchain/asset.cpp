#define __STDC_CONSTANT_MACROS
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <fc/exception/exception.hpp>
#include <fc/crypto/bigint.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <sstream>
#include <cstdint>



/** more base 10 digits is beyond the precision of 64 bits */
#define BASE10_PRECISION  UINT64_C(100000000000000)


namespace bts { namespace blockchain {



  asset::operator std::string()const
  {
     return fc::to_string(amount);
  }

  asset& asset::operator += ( const asset& o )
  {
     FC_ASSERT( asset_id == o.asset_id );

     auto old = *this;
     amount += o.amount;

     if( amount < old.amount ) 
     {
       FC_THROW_EXCEPTION( addition_overflow, "asset addition overflowed  ${a} + ${b} = ${c}", 
                            ("a", old)("b",o)("c",*this) );
     }
     return *this;
  }

  asset  asset::operator *  ( const fc::uint128_t& fix6464 )const
  {
      fc::bigint bi(amount);
      bi *= fix6464;
      bi /= BTS_PRICE_PRECISION; //>>= 64;
      return asset( fc::uint128(bi).high_bits(), asset_id );
  }

  asset& asset::operator -= ( const asset& o )
  {
     FC_ASSERT( asset_id == o.asset_id );
     auto old = *this;;
     amount -= o.amount;
     if( amount > old.amount ) 
     {
        FC_THROW_EXCEPTION( addition_underthrow, "asset addition underflow  ${a} - ${b} = ${c}", 
                            ("a", old)("b",o)("c",*this) );
     }
     return *this;
  }

  const fc::uint128& price::one()
  {
     static fc::uint128_t o = fc::uint128(1,0);
     return o;
  }
  const fc::uint128& price::infinite()
  {
      static fc::uint128 i(-1);
      return i;
  }

  price::price( const std::string& s )
  { try {
     
     std::stringstream ss(s);
     std::string price_int;
     char div;
     int quote_int;
     int base_int;
     char dot;
     int64_t int_part;
     std::string fraction_part;
     ss >> int_part >> dot >> fraction_part >> quote_int >> div >> base_int;

     std::string fract_str( fc::uint128( BTS_PRICE_PRECISION ) );
     for( uint32_t i =0 ; i < fraction_part.size(); ++i )
     {
        fract_str[i+1] = fraction_part[i];
     }
     
     ratio = fc::uint128(int_part) * BTS_PRICE_PRECISION + fc::uint128(fract_str) - BTS_PRICE_PRECISION;

     quote_asset_id = quote_int;
     base_asset_id  = base_int;
  } FC_RETHROW_EXCEPTIONS( warn, "" ) }

  void price::set_ratio_from_string( const std::string& ratio_str )
  {
     std::stringstream ss(ratio_str);
     std::string price_int;
     char dot;
     int64_t int_part;
     std::string fraction_part;
     ss >> int_part >> dot >> fraction_part;

     std::string fract_str( fc::uint128( BTS_PRICE_PRECISION ) );
     for( uint32_t i =0 ; i < fraction_part.size(); ++i )
     {
        fract_str[i+1] = fraction_part[i];
     }
     
     ratio = fc::uint128(int_part) * BTS_PRICE_PRECISION + fc::uint128(fract_str) - BTS_PRICE_PRECISION;
  }

  price::price( double a, asset_id_type q, asset_id_type b )
  {
     FC_ASSERT( q > b, "${quote} > ${base}", ("quote",q)("base",b) );

     uint64_t high_bits = uint64_t(a);
     double fract_part = a - high_bits;
     //uint64_t low_bits = uint64_t(-1)*fract_part;
     //ratio = fc::uint128( high_bits, low_bits );
     ratio = (fc::uint128( high_bits ) * BTS_PRICE_PRECISION) + (int64_t((fract_part) * BTS_PRICE_PRECISION));
     base_asset_id = b;
     quote_asset_id = q;
  }

  price::operator double()const
  {
     return double(ratio.high_bits()) + double(ratio.low_bits()) / double(uint64_t(-1));
  }

  std::string price::ratio_string()const
  {
     std::string full_int = std::string( ratio );
     std::stringstream ss;
     ss <<std::string( ratio / BTS_PRICE_PRECISION ); 
     ss << '.';
     ss << std::string( (ratio % BTS_PRICE_PRECISION) + BTS_PRICE_PRECISION ).substr(1);

     auto number = ss.str();
     while(  number.back() == '0' ) number.pop_back();

     return number;
  }
  price::operator std::string()const
  { try {
     auto number = ratio_string();

     number += ' ' + fc::to_string(int64_t(quote_asset_id.value));
     number +=  '/' + fc::to_string(int64_t(base_asset_id.value)); 

     return number;
  } FC_RETHROW_EXCEPTIONS( warn, "" )}

  /**
   *  A price will reorder the asset types such that the
   *  asset type with the lower enum value is always the
   *  denominator.  Therefore  bts/usd and  usd/bts will
   *  always result in a price measured in usd/bts because
   *  asset::bts <  asset::usd.
   */
  price operator / ( const asset& a, const asset& b )
  {
    try 
    {
        if( a.asset_id == b.asset_id )
           FC_CAPTURE_AND_THROW( asset_divide_by_self );

        price p;
        auto l = a; auto r = b;
        if( l.asset_id < r.asset_id ) { std::swap(l,r); }
        ilog( "${a} / ${b}", ("a",l)("b",r) );

        if( r.amount == 0 )
           FC_CAPTURE_AND_THROW( asset_divide_by_zero, (r) );

        p.base_asset_id = r.asset_id;
        p.quote_asset_id = l.asset_id;

        fc::bigint bl = l.amount;
        fc::bigint br = r.amount;
        fc::bigint result = (bl * fc::bigint(BTS_PRICE_PRECISION)) / br;

        p.ratio = result;
        return p;
    } FC_RETHROW_EXCEPTIONS( warn, "${a} / ${b}", ("a",a)("b",b) );
  }


  /**
   *  Assuming a.type is either the numerator.type or denominator.type in
   *  the price equation, return the number of the other asset type that
   *  could be exchanged at price p.
   *
   *  ie:  p = 3 usd/bts & a = 4 bts then result = 12 usd
   *  ie:  p = 3 usd/bts & a = 4 usd then result = 1.333 bts 
   */
  asset operator * ( const asset& a, const price& p )
  {
    try {
        if( a.asset_id == p.base_asset_id )
        {
            fc::bigint ba( a.amount ); // 64.64
            fc::bigint r( p.ratio ); // 64.64

            auto amnt = ba * r; //  128.128
            amnt /= BTS_PRICE_PRECISION; // 128.64 
            auto lg2 = amnt.log2();
            if( lg2 >= 128 )
            {
               FC_THROW_EXCEPTION( addition_overflow, "overflow ${a} * ${p}", ("a",a)("p",p) );
            }

            asset rtn;
            rtn.amount = amnt.to_int64();
            rtn.asset_id = p.quote_asset_id;

            ilog( "${a} * ${p} => ${rtn}", ("a", a)("p",p )("rtn",rtn) );
            return rtn;
        }
        else if( a.asset_id == p.quote_asset_id )
        {
            fc::bigint amt( a.amount ); // 64.64
            amt *= BTS_PRICE_PRECISION; //<<= 64;  // 64.128
            fc::bigint pri( p.ratio ); // 64.64

            auto result = amt / pri;  // 64.64
//            ilog( "amt: ${amt} / ${pri}", ("amt",string(amt))("pri",string(pri) ) );
 //           ilog( "${r}", ("r",string(result) ) );

            auto lg2 = result.log2();
            if( lg2 >= 128 )
            {
             //  wlog( "." );
               FC_THROW_EXCEPTION( addition_overflow, 
                                    "overflow ${a} / ${p} = ${r} lg2 = ${l}", 
                                    ("a",a)("p",p)("r", std::string(result)  )("l",lg2) );
            }
          //  result += 5000000000; // TODO: evaluate this rounding factor..
            asset r;
            r.amount    = result.to_int64();
            r.asset_id  = p.base_asset_id;
            ilog( "r.amount = ${r}", ("r",r.amount) );
            ilog( "${a} * ${p} => ${rtn}", ("a", a)("p",p )("rtn",r) );
            return r;
        }
        FC_THROW_EXCEPTION( asset_type_mismatch, "type mismatch multiplying asset ${a} by price ${p}", 
                                            ("a",a)("p",p) );
    } FC_RETHROW_EXCEPTIONS( warn, "type mismatch multiplying asset ${a} by price ${p}", 
                                        ("a",a)("p",p) );

  }




} } // bts::blockchain
namespace fc
{
/*
   void to_variant( const bts::blockchain::asset& var,  variant& vo )
   {
     vo = std::string(var);
   }
   void from_variant( const variant& var,  bts::blockchain::asset& vo )
   {
     vo = bts::blockchain::asset( var.as_string() );
   }
*/
   void to_variant( const bts::blockchain::price& var,  variant& vo )
   {
      fc::mutable_variant_object obj("ratio",var.ratio_string());
      obj( "quote_asset_id", var.quote_asset_id );
      obj( "base_asset_id", var.base_asset_id );
      vo = std::move(obj);
   }
   void from_variant( const variant& var,  bts::blockchain::price& vo )
   {
     auto obj = var.get_object();
     vo.set_ratio_from_string( obj["ratio"].as_string() );
     from_variant( obj["quote_asset_id"], vo.quote_asset_id );
     from_variant( obj["base_asset_id"], vo.base_asset_id );
   }
}
