#include <bts/btsx/wallet.hpp>
#include <bts/btsx/outputs.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>
#include <iostream>

namespace bts { namespace btsx {

   namespace detail 
   { 
      class wallet_impl 
      {

      };
   }

   wallet::wallet()
   :my( new detail::wallet_impl() )
   {
   }

   wallet::~wallet()
   {
   }

   void wallet::dump_output( const trx_output& out )
   {
       switch( out.claim_func )
       {
          case claim_by_bid:
          {
             auto claim = out.as<claim_by_bid_output>();
             std::cerr<<std::string(out.amount)<<" ";
             std::cerr<<"claim_by_bid ";
             std::cerr<< std::string(claim.pay_address);
             std::cerr<< " price: ";
             std::cerr<< std::string(claim.ask_price);
             break;
          }
          case claim_by_long:
          {
             auto claim = out.as<claim_by_long_output>();
             std::cerr<<std::string(out.amount)<<" ";
             std::cerr<<"claim_by_long ";
             std::cerr<< std::string(claim.pay_address);
             std::cerr<< " price: ";
             std::cerr<< std::string(claim.ask_price);
             break;
          }
          case claim_by_cover:
          {
             auto claim = out.as<claim_by_cover_output>();
             std::cerr<<std::string(out.amount)<<" ";
             std::cerr<<"claim_by_cover ";
             std::cerr<< std::string(claim.owner);
             std::cout<< " payoff: "<< std::string( claim.payoff );
             std::cout<< " call price: "<< std::string( claim.get_call_price( out.amount ) );
             break;
          }
       }
   }

   void wallet::scan_output( const trx_output& out, const output_reference& out_ref, 
                             const bts::wallet::output_index& oidx )
   { try {
      switch( out.claim_func )
      {
         case claim_by_bid:
         {
            if( is_my_address( out.as<claim_by_bid_output>().pay_address ) )
                cache_output( out, out_ref, oidx );
            break;
         }
         case claim_by_long:
         {
            if( is_my_address( out.as<claim_by_long_output>().pay_address ) )
                   cache_output( out, out_ref, oidx );
            break;
         }
         case claim_by_cover:
         {
            if( is_my_address( out.as<claim_by_cover_output>().owner ) )
                   cache_output( out, out_ref, oidx );
            break;
         }
         default:
            FC_ASSERT( !"Invalid Claim Type" );
            break;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

} } // bts::btsx
