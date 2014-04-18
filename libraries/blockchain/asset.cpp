#define __STDC_CONSTANT_MACROS
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/exception/exception.hpp>
#include <fc/crypto/bigint.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <sstream>
#include <cstdint>



/** more base 10 digits is beyond the precision of 64 bits */
#define BASE10_PRECISION  UINT64_C(100000000000000)


namespace bts { namespace blockchain {


  uint64_t asset::get_rounded_amount()const
  {
     return amount;
  }

  asset::operator std::string()const
  {
     return fc::to_string(amount);
  }

  asset& asset::operator += ( const asset& o )
  {
     FC_ASSERT( unit == o.unit );

     auto old = *this;
     amount += o.amount;

     if( amount < old.amount ) 
     {
       FC_THROW_EXCEPTION( exception, "asset addition overflowed  ${a} + ${b} = ${c}", 
                            ("a", old)("b",o)("c",*this) );
     }
     return *this;
  }

  asset  asset::operator *  ( const fc::uint128_t& fix6464 )const
  {
      fc::bigint bi(amount);
      bi *= fix6464;
      bi >>= 64;
      return asset( fc::uint128(bi).high_bits(), unit );
  }

  asset& asset::operator -= ( const asset& o )
  {
     FC_ASSERT( unit == o.unit );
     auto old = *this;;
     amount -= o.amount;
     if( amount > old.amount ) 
     {
        FC_THROW_EXCEPTION( exception, "asset addition underflow  ${a} - ${b} = ${c}", 
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
  {
     std::stringstream ss(s);
     std::string quote, base, units;
     double a;
     ss >> a >> units; 
     quote = units.substr(0,3);
     base = units.substr(4,3);
     ratio      = fc::uint128( uint64_t(a), uint64_t(-1) * (a - uint64_t(a)) ); 
     quote_unit = fc::variant(quote).as<asset::unit_type>();
     base_unit  = fc::variant(base).as<asset::unit_type>();
  }

  price::price( double a, asset::unit_type q, asset::unit_type b )
  {
     FC_ASSERT( q > b, "${quote} > ${base}", ("quote",q)("base",b) );

     uint64_t high_bits = uint64_t(a);
     double fract_part = a - high_bits;
     uint64_t low_bits = uint64_t(-1)*fract_part;
     ratio = fc::uint128( high_bits, low_bits );
     base_unit = b;
     quote_unit = q;
  }

  price::operator double()const
  {
     return double(ratio.high_bits()) + double(ratio.low_bits()) / double(uint64_t(-1));
  }

  price::operator std::string()const
  {
     uint64_t integer = ratio.high_bits();
     fc::uint128 one(1,0);

     fc::uint128 fraction(0,ratio.low_bits());
     fraction *= BASE10_PRECISION;
     fraction /= one;
     fraction += BASE10_PRECISION;
     if( fraction.to_uint64()  % 10 >= 5 )
     {
        fraction += 10 - (fraction.to_uint64()  % 10);
        integer += ((fraction / BASE10_PRECISION) - 1).to_uint64();
     }
     std::string s = fc::to_string(integer);
     s += ".";
     std::string frac(fraction);
     s += frac.substr(1,frac.size()-2);
     s += " " + fc::to_string(quote_unit);
     s += "/" + fc::to_string(base_unit); 
     return s;
  }

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
        ilog( "${a} / ${b}", ("a",a)("b",b) );
        price p;
        auto l = a; auto r = b;
        if( l.unit < r.unit ) { std::swap(l,r); }
        ilog( "${a} / ${b}", ("a",l)("b",r) );

        p.base_unit = r.unit;
        p.quote_unit = l.unit;

        fc::bigint bl = l.amount;
        fc::bigint br = r.amount;
        fc::bigint result = (bl <<= 64) / br;

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
        if( a.unit == p.base_unit )
        {
            fc::bigint ba( a.amount ); // 64.64
            fc::bigint r( p.ratio ); // 64.64
            //fc::uint128 ba_test = ba; 

            auto amnt = ba * r; //  128.128
            amnt >>= 64; // 128.64 
            auto lg2 = amnt.log2();
            if( lg2 >= 128 )
            {
               FC_THROW_EXCEPTION( exception, "overflow ${a} * ${p}", ("a",a)("p",p) );
            }
         //   amnt += 5000000000; // TODO:evaluate this rounding factor... 

            asset rtn;
            rtn.amount = amnt;
            rtn.unit = p.quote_unit;

            ilog( "${a} * ${p} => ${rtn}", ("a", a)("p",p )("rtn",rtn) );
            return rtn;
        }
        else if( a.unit == p.quote_unit )
        {
            fc::bigint amt( a.amount ); // 64.64
            amt <<= 64;  // 64.128
            fc::bigint pri( p.ratio ); // 64.64

            auto result = amt / pri;  // 64.64
            //auto test_result = result;
            //ilog( "test result: ${r}", ("r", std::string(test_result >>= 60) ) );
            auto lg2 = result.log2();
            if( lg2 >= 128 )
            {
             //  wlog( "." );
               FC_THROW_EXCEPTION( exception, 
                                    "overflow ${a} / ${p} = ${r} lg2 = ${l}", 
                                    ("a",a)("p",p)("r", std::string(result)  )("l",lg2) );
            }
          //  result += 5000000000; // TODO: evaluate this rounding factor..
            asset r;
            r.amount = result;
            r.unit   = p.base_unit;
            ilog( "${a} * ${p} => ${rtn}", ("a", a)("p",p )("rtn",r) );
            return r;
        }
        FC_THROW_EXCEPTION( exception, "type mismatch multiplying asset ${a} by price ${p}", 
                                            ("a",a)("p",p) );
    } FC_RETHROW_EXCEPTIONS( warn, "type mismatch multiplying asset ${a} by price ${p}", 
                                        ("a",a)("p",p) );

  }




} } // bts::blockchain
/*
namespace fc
{
   void to_variant( const bts::blockchain::asset& var,  variant& vo )
   {
     vo = std::string(var);
   }
   void from_variant( const variant& var,  bts::blockchain::asset& vo )
   {
     vo = bts::blockchain::asset( var.as_string() );
   }
   void to_variant( const bts::blockchain::price& var,  variant& vo )
   {
     vo = std::string(var);
   }
   void from_variant( const variant& var,  bts::blockchain::price& vo )
   {
     vo = bts::blockchain::price( var.as_string() );
   }
}
*/
