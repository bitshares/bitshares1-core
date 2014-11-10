#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

#include <algorithm>
#include <locale>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   optional<string> chain_interface::get_parent_account_name( const string& account_name )const
   {
      const size_t pos = account_name.find( '.' );
      if( pos != string::npos )
          return account_name.substr( pos + 1 );
      return optional<string>();
   }

   bool chain_interface::is_valid_account_name( const std::string& str )const
   {
      if( str.size() < BTS_BLOCKCHAIN_MIN_NAME_SIZE ) return false;
      if( str.size() > BTS_BLOCKCHAIN_MAX_NAME_SIZE ) return false;
      if( !isalpha(str[0]) ) return false;
      if ( !isalnum(str[str.size()-1]) || isupper(str[str.size()-1]) ) return false;

      std::string subname(str);
      std::string supername;
      int dot = str.find('.');
      if( dot != std::string::npos )
      {
        subname = str.substr(0, dot);
        //There is definitely a remainder; we checked above that the last character is not a dot
        supername = str.substr(dot+1);
      }

      if ( !isalnum(subname[subname.size()-1]) || isupper(subname[subname.size()-1]) ) return false;
      for( const auto& c : subname )
      {
          if( isalnum(c) && !isupper(c) ) continue;
          else if( c == '-' ) continue;
          else return false;
      }

      if( supername.empty() )
        return true;
      return is_valid_account_name(supername);
   }

   bool chain_interface::is_valid_symbol_name( const string& name )const
   {
#ifndef WIN32
#warning [SOFTFORK] Remove after BTS_V0_4_24_FORK_BLOCK_NUM has passed
#endif
       if( get_head_block_num() < BTS_V0_4_24_FORK_BLOCK_NUM )
       {
           if( name.size() > 5 )
               return false;
       }

      FC_ASSERT( name != "BTSX" );

      if( name.size() > BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE )
         return false;
      if( name.size() < BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE )
         return false;
      std::locale loc;
      for( const auto& c : name )
         if( !std::isalnum(c,loc) || !std::isupper(c,loc) )
            return false;
      return true;
   }

   // Starting 2014-11-06, delegates are issued max 50 shares per block produced, and this value is halved every 4 years
   // just like in Bitcoin
   share_type chain_interface::get_max_delegate_pay_per_block()const
   {
       static const time_point_sec start_timestamp = time_point_sec( 1415188800 ); // 2014-11-06 00:00:00 UTC
       static const uint32_t seconds_per_period = fc::days( 4 * 365 ).to_seconds(); // Ignore leap years, leap seconds, etc.

       const time_point_sec now = this->now();
       FC_ASSERT( now >= start_timestamp );
       const uint32_t elapsed_time = (now - start_timestamp).to_seconds();

       const uint32_t num_full_periods = elapsed_time / seconds_per_period;

       share_type pay_per_block = BTS_MAX_DELEGATE_PAY_PER_BLOCK;
       for( uint32_t i = 0; i < num_full_periods; ++i )
           pay_per_block /= 2;

       return pay_per_block;
   }

   share_type chain_interface::get_delegate_registration_fee( uint8_t pay_rate )const
   {
       if( get_head_block_num() < BTS_V0_4_24_FORK_BLOCK_NUM )
           return get_delegate_registration_fee_v1( pay_rate );

       static const uint32_t blocks_per_two_weeks = 14 * BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
       const share_type max_total_pay_per_two_weeks = blocks_per_two_weeks * get_max_delegate_pay_per_block();
       const share_type max_pay_per_two_weeks = max_total_pay_per_two_weeks / BTS_BLOCKCHAIN_NUM_DELEGATES;
       const share_type registration_fee = (max_pay_per_two_weeks * pay_rate) / 100;
       FC_ASSERT( registration_fee > 0 );
       return registration_fee;
   }

   share_type chain_interface::get_asset_registration_fee( uint8_t symbol_length )const
   {
       if( get_head_block_num() < BTS_V0_4_24_FORK_BLOCK_NUM )
           return get_asset_registration_fee_v1();

       // TODO: Add #define's for these fixed prices
       static const share_type long_symbol_price = 500 * BTS_BLOCKCHAIN_PRECISION; // $10 at $0.02/XTS
       static const share_type short_symbol_price = 1000 * long_symbol_price;
       FC_ASSERT( long_symbol_price > 0 );
       FC_ASSERT( short_symbol_price > long_symbol_price );
       return symbol_length <= 5 ? short_symbol_price : long_symbol_price;
   }

   asset_id_type chain_interface::last_asset_id()const
   {
       return get_property( chain_property_enum::last_asset_id ).as<asset_id_type>();
   }

   asset_id_type chain_interface::new_asset_id()
   {
      auto next_id = last_asset_id() + 1;
      set_property( chain_property_enum::last_asset_id, next_id );
      return next_id;
   }

   account_id_type chain_interface::last_account_id()const
   {
       return get_property( chain_property_enum::last_account_id ).as<account_id_type>();
   }

   account_id_type chain_interface::new_account_id()
   {
      auto next_id = last_account_id() + 1;
      set_property( chain_property_enum::last_account_id, next_id );
      return next_id;
   }

#if 0
   proposal_id_type   chain_interface::last_proposal_id()const
   {
       return get_property( chain_property_enum::last_proposal_id ).as<proposal_id_type>();
   }

   proposal_id_type   chain_interface::new_proposal_id()
   {
      auto next_id = last_proposal_id() + 1;
      set_property( chain_property_enum::last_proposal_id, next_id );
      return next_id;
   }
#endif

   vector<account_id_type> chain_interface::get_active_delegates()const
   { try {
      return get_property( active_delegate_list_id ).as<std::vector<account_id_type>>();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_interface::set_active_delegates( const std::vector<account_id_type>& delegate_ids )
   {
      set_property( active_delegate_list_id, fc::variant( delegate_ids ) );
   }

   bool chain_interface::is_active_delegate( const account_id_type& id )const
   { try {
      const auto active = get_active_delegates();
      return active.end() != std::find( active.begin(), active.end(), id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("id",id) ) }

   double chain_interface::to_pretty_price_double( const price& price_to_pretty_print )const
   {
      auto obase_asset = get_asset_record( price_to_pretty_print.base_asset_id );
      if( !obase_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.base_asset_id) );

      auto oquote_asset = get_asset_record( price_to_pretty_print.quote_asset_id );
      if( !oquote_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.quote_asset_id) );

      return fc::variant(string(price_to_pretty_print.ratio * obase_asset->get_precision() / oquote_asset->get_precision())).as_double() / (BTS_BLOCKCHAIN_MAX_SHARES*1000);
   }

   string chain_interface::to_pretty_price( const price& price_to_pretty_print )const
   { try {
      auto obase_asset = get_asset_record( price_to_pretty_print.base_asset_id );
      if( !obase_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.base_asset_id) );

      auto oquote_asset = get_asset_record( price_to_pretty_print.quote_asset_id );
      if( !oquote_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.quote_asset_id) );

      auto tmp = price_to_pretty_print;
      tmp.ratio *= obase_asset->get_precision();
      tmp.ratio /= oquote_asset->get_precision();

      return tmp.ratio_string() + " " + oquote_asset->symbol + " / " + obase_asset->symbol;
   } FC_CAPTURE_AND_RETHROW( (price_to_pretty_print) ) }

   asset chain_interface::to_ugly_asset(const std::string& amount, const std::string& symbol) const
   { try {
      auto record = get_asset_record( symbol );
      if( !record ) FC_CAPTURE_AND_THROW( unknown_asset_symbol, (symbol) );

      auto decimal = amount.find(".");
      if( decimal == string::npos )
         return asset(atoll(amount.c_str()) * record->precision, record->id);

      share_type whole = atoll(amount.substr(0, decimal).c_str()) * record->precision;
      string fraction_string = amount.substr(decimal+1);
      share_type fraction = atoll(fraction_string.c_str());

      if( fraction_string.empty() || fraction <= 0 )
         return asset(whole, record->id);

      while( fraction < record->precision )
         fraction *= 10;
      while( fraction > record->precision )
         fraction /= 10;
      while( fraction_string.size() && fraction_string[0] == '0')
      {
         fraction /= 10;
         fraction_string.erase(0, 1);
      }
      return asset(whole >= 0? whole + fraction : whole - fraction, record->id);
       } FC_CAPTURE_AND_RETHROW( (amount)(symbol) ) }

   price chain_interface::to_ugly_price(const std::string& price_string,
                                        const std::string& base_symbol,
                                        const std::string& quote_symbol,
                                        bool do_precision_dance) const
   { try {
      auto base_record = get_asset_record(base_symbol);
      auto quote_record = get_asset_record(quote_symbol);
      if( !base_record ) FC_CAPTURE_AND_THROW( unknown_asset_symbol, (base_symbol) );
      if( !quote_record ) FC_CAPTURE_AND_THROW( unknown_asset_symbol, (quote_symbol) );

      price ugly_price(price_string + " " + std::to_string(quote_record->id) + " / " + std::to_string(base_record->id));
      if( do_precision_dance )
      {
         ugly_price.ratio *= quote_record->get_precision();
         ugly_price.ratio /= base_record->get_precision();
      }
      return ugly_price;
   } FC_CAPTURE_AND_RETHROW( (price_string)(base_symbol)(quote_symbol) ) }

   string chain_interface::to_pretty_asset( const asset& a )const
   {
      const auto oasset = get_asset_record( a.asset_id );
      const auto amount = ( a.amount >= 0 ) ? a.amount : -a.amount;
      if( oasset.valid() )
      {
         const auto precision = oasset->get_precision();
         string decimal = fc::to_string( precision + ( amount % precision ) );
         decimal[0] = '.';
         const auto str = fc::to_pretty_string( amount / precision ) + decimal + " " + oasset->symbol;
         if( a.amount < 0 ) return "-" + str;
         return str;
      }
      else
      {
         return fc::to_pretty_string( a.amount ) + " ???";
      }
   }

   int64_t chain_interface::get_required_confirmations()const
   {
      return get_property( confirmation_requirement ).as_int64();
   }

   void chain_interface::set_dirty_markets( const std::set<std::pair<asset_id_type, asset_id_type>>& d )
   {
       set_property( dirty_markets, fc::variant( d ) );
   }

   std::set<std::pair<asset_id_type, asset_id_type>> chain_interface::get_dirty_markets()const
   {
       try
       {
           return get_property( dirty_markets ).as<std::set<std::pair<asset_id_type, asset_id_type>>>();
       }
       catch( ... )
       {
       }
       return std::set<std::pair<asset_id_type, asset_id_type>>();
   }

} } // bts::blockchain
