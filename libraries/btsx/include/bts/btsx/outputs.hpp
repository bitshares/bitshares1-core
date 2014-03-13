#pragma once
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/outputs.hpp>
#include <fc/time.hpp>

namespace bts { namespace btsx {
   using namespace bts::blockchain;

   enum claim_type_enum
   {
      claim_by_bid         = 10, ///< someone makes an acceptable bid
      claim_by_long        = 11, ///< someone agrees to go long against a short
      claim_by_cover       = 12, ///< someone covers a short, freeing collateral
      claim_by_cover_bid   = 13, ///< someone wanting to use their collateral to place a bid
      claim_by_opt_execute = 14, ///< someone executes an option
   };

   /**
    *  There are only two ways to claim a bid output, as part of a valid market
    *  transaction occuring below the ask price per unit or as part of a cancel
    *  operation.
    */
   struct claim_by_bid_input         { static const claim_type_enum type; };
   struct claim_by_long_input        { static const claim_type_enum type; };
   struct claim_by_cover_input       { static const claim_type_enum type; };
   struct claim_by_cover_bid_input   { static const claim_type_enum type; };
   struct claim_by_opt_execute_input { static const claim_type_enum type; };

   /** 
    *  The output of the claim trx specifies the amount of a 
    *  specified asset unit we are offering in exchange for 
    *  the ask unit where the price per unit specifies the
    *  minimum (or maximum) exchange rate depending upon whether
    *  the ask_unit is the base or quote unit of the price.
    **/
   struct claim_by_bid_output
   {
      static const claim_type_enum type;
      claim_by_bid_output(){}
      claim_by_bid_output( const address& pay_addr, const price& ask )
      :pay_address(pay_addr),ask_price(ask){}

      bool is_bid(asset::type out_unit)const;
      bool is_ask(asset::type out_unit)const;

      address  pay_address; // where to send ask_unit (or cancel sig)
      price    ask_price;   // price base per unit

      bool operator == ( const claim_by_bid_output& other )const;
   };

   /**
    *  This output may be spent via signature (to cancel) or via a market action
    *  of a miner.
    *
    *  Assumptions:
    *     trx_output.unit   ==  bts  
    *     trx_output.amount == amount of bts held as collateral
    *     ask_price         == price to convert to usd
    */
   struct claim_by_long_output
   {
      static const claim_type_enum type;
      claim_by_long_output(){}

      claim_by_long_output( const address& pay_addr, const price& ask )
      :pay_address(pay_addr),ask_price(ask){}

      bool operator == ( const claim_by_long_output& other )const;
      address     pay_address; ///< where to send ask_unit (or cancel sig)
      price       ask_price;   ///< price per unit (base must be bts)
   };

   /**
    *  Given a transaction that destroys payoff_amount of payoff_unit and is
    *  signed by owner, then this output can be spent.  Alternatively, the
    *  owner could transfer the short position to a new owner.
    *
    *  This position could also be spent as part of a margin call.
    *
    *  Assumptions:
    *    trx_output.unit = bts
    *    trx_output.amount = total collateral held
    *
    *    payoff_amount / unit counts as a 'negative input' to the transaction.
    */
   struct claim_by_cover_output
   {
      static const claim_type_enum type;

      claim_by_cover_output( const asset& pay, const address& own )
      :payoff(pay), owner(own){}

      claim_by_cover_output(){}

      price get_call_price( asset collat )const;
      bool operator == ( const claim_by_cover_output& other )const;

      asset      payoff;
      address    owner;
   };

   /**
    *  This output may be claimed by the optionor after the first block with a timestamp
    *  after the expire_time or by the optionee at any point prior to the expire_time provided
    *  it is part of a transaction that pays strike_amount of strike_unit to the optionor. 
    *
    *  Assumptions:
    *    trx.output.unit = the unit that may be bought provided strike unit/amount are provided
    *    trx.output.amount = the amount that may be bought
    *    If the option is exersized, the dividends earned go to the person who claims it.
    *
    *    Option may be rolled forward by either the optionor or optioneee, failure to
    *    roll the option forward results in the same 5% fee plus dividend forfeiture as
    *    any other output balance.
    */
   struct claim_by_opt_execute_output
   {
      static const claim_type_enum type;

      address             optionor; // who to pay for this option (also who may cancel this offer)
      fc::time_point_sec  expire_time;   
      asset               strike_amount;
      address             optionee; // who owns the right to this option.
   };

} } // bts::btxs

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::btsx::claim_by_bid_output, (pay_address)(ask_price) )
FC_REFLECT( bts::btsx::claim_by_long_output, (pay_address)(ask_price) )
FC_REFLECT( bts::btsx::claim_by_cover_output, (payoff)(owner) )
FC_REFLECT( bts::btsx::claim_by_opt_execute_output, (optionor)(expire_time)(strike_amount)(optionee) )

FC_REFLECT( bts::btsx::claim_by_bid_input,          BOOST_PP_SEQ_NIL )
FC_REFLECT( bts::btsx::claim_by_long_input,         BOOST_PP_SEQ_NIL )
FC_REFLECT( bts::btsx::claim_by_cover_input,        BOOST_PP_SEQ_NIL )
FC_REFLECT( bts::btsx::claim_by_opt_execute_input,  BOOST_PP_SEQ_NIL )

FC_REFLECT_ENUM( bts::btsx::claim_type_enum, 
         (claim_by_bid)
         (claim_by_long)
         (claim_by_cover)
         (claim_by_cover_bid)
         (claim_by_opt_execute) 
      )

