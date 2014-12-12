#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <fc/io/raw_variant.hpp>

#include <algorithm>
#include <locale>

namespace bts { namespace blockchain {

   optional<string> chain_interface::get_parent_account_name( const string& account_name )const
   {
      const size_t pos = account_name.find( '.' );
      if( pos != string::npos )
          return account_name.substr( pos + 1 );
      return optional<string>();
   }

   bool chain_interface::is_valid_account_name( const string& name )const
   { try {
      if( name.size() < BTS_BLOCKCHAIN_MIN_NAME_SIZE ) return false;
      if( name.size() > BTS_BLOCKCHAIN_MAX_NAME_SIZE ) return false;
      if( !isalpha(name[0]) ) return false;
      if ( !isalnum(name[name.size()-1]) || isupper(name[name.size()-1]) ) return false;

      string subname(name);
      string supername;
      int dot = name.find('.');
      if( dot != string::npos )
      {
        subname = name.substr(0, dot);
        //There is definitely a remainder; we checked above that the last character is not a dot
        supername = name.substr(dot+1);
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
   } FC_CAPTURE_AND_RETHROW( (name) ) }

   /**
    *  Symbol names must be Alpha Numeric and may have a single '.' in the name that cannot be
    *  the first or last.
    */
   bool chain_interface::is_valid_symbol_name( const string& symbol )const
   { try {
       if( symbol.size() < BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE || symbol.size() > BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE )
           return false;

       int dots = 0;
       for( const char& c : symbol )
       {
           if( c == '.' )
           {
              if( ++dots > 1 )
               return false;
           }
           else if( !std::isalnum( c, std::locale::classic() ) || !std::isupper( c, std::locale::classic() ) )
               return false;
       }
       if( symbol.back() == '.' ) return false;
       if( symbol.front() == '.' ) return false;

       if( symbol.size() >= 3 && symbol.find( "BIT" ) == 0 )
           return false;

       return true;
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   // Starting 2014-11-06, delegates are issued max 50 shares per block produced, and this value is halved every 4 years
   // just like in Bitcoin
   share_type chain_interface::get_max_delegate_pay_issued_per_block()const
   {
       share_type pay_per_block = BTS_MAX_DELEGATE_PAY_PER_BLOCK;

       static const time_point_sec start_timestamp = time_point_sec( 1415188800 ); // 2014-11-06 00:00:00 UTC
       static const uint32_t seconds_per_period = fc::days( 4 * 365 ).to_seconds(); // Ignore leap years, leap seconds, etc.

       const time_point_sec now = this->now();
       if( now >= start_timestamp )
       {
           const uint32_t elapsed_time = (now - start_timestamp).to_seconds();
           const uint32_t num_full_periods = elapsed_time / seconds_per_period;
           for( uint32_t i = 0; i < num_full_periods; ++i )
               pay_per_block /= 2;
       }

       return pay_per_block;
   }

   share_type chain_interface::get_delegate_registration_fee( uint8_t pay_rate )const
   {
       static const uint32_t blocks_per_two_weeks = 14 * BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
       const share_type max_total_pay_per_two_weeks = blocks_per_two_weeks * get_max_delegate_pay_issued_per_block();
       const share_type max_pay_per_two_weeks = max_total_pay_per_two_weeks / BTS_BLOCKCHAIN_NUM_DELEGATES;
       const share_type registration_fee = (max_pay_per_two_weeks * pay_rate) / 100;
       FC_ASSERT( registration_fee > 0 );
       return registration_fee;
   }

   share_type chain_interface::get_asset_registration_fee( uint8_t symbol_length )const
   {
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

   object_id_type chain_interface::last_object_id()const
   {
       return get_property( chain_property_enum::last_object_id ).as<object_id_type>();
   }

   object_id_type chain_interface::new_object_id( obj_type type )
   {
      auto last_id = last_object_id();
      auto tmp = object_record( type, last_id );
      tmp.set_id( tmp.type(), tmp.short_id() + 1 );
      auto next_id = tmp._id;
      set_property( chain_property_enum::last_object_id, object_id_type( next_id ) );
      return next_id;
   }

   // Get an object for whom get_object_condition(o) will not throw and will represent
   // the condition that is also the owner for this given object
   object_id_type       chain_interface::get_owner_object( const object_id_type& obj )
   {
       FC_ASSERT(!"unimplemented");
   }

   multisig_condition   chain_interface::get_object_condition( const object_record& obj, int depth )
   { try {
       if( depth >= 100 )//BTS_OWNER_DEPENDENCY_MAX_DEPTH )
           FC_ASSERT(!"Cannot determine object condition.");
       multisig_condition condition;
       switch( obj.type() )
       {
           case( obj_type::base_object ):
           {
               if( obj.owner_object == obj._id )
                   return obj._owners;
               else
                   return get_object_condition( obj.owner_object, depth+1 );
           }
           case( obj_type::edge_object ):
           {
               ilog("@n object: ${o}", ("o", obj));
               const edge_record& edge = obj.as<edge_record>();
               ilog("@n edge: ${e}", ("e", edge));
               auto from_object = get_object_record( edge.from );
               FC_ASSERT( from_object.valid(), "Unrecognized from object.");
               return get_object_condition( *from_object, depth+1 );
           }
           case( obj_type::account_object ):
           {
               auto account_id = obj.short_id();
               auto oacct = get_account_record( account_id );
               FC_ASSERT( oacct.valid(), "No such account object!");
               condition.owners.insert( oacct->owner_address() );
               condition.required = 1;
               return condition;
           }
           case( obj_type::asset_object ):
           {
               auto oasset = get_asset_record( obj.short_id() );
               FC_ASSERT( oasset.valid(), "No such asset!" );
               if( oasset->issuer_account_id > 0 )
               {
                   auto oacct = get_account_record( oasset->issuer_account_id );
                   FC_ASSERT(!"This asset has an issuer but the issuer account doens't exist. Crap!");
                   condition.owners.insert( oacct->owner_address() );
                   condition.required = 1;
                   return condition;
               }
               else
               {
                   FC_ASSERT(!"That asset has no issuer!");
               }
           }
           default:
           {
               FC_ASSERT(!"I don't know how to get the condition for this object type!");
           }
       }
       FC_ASSERT(!"This code path should not happen.");
   } FC_CAPTURE_AND_RETHROW( (obj.short_id())(obj.type())(obj) ) }

    oedge_record               chain_interface::get_edge( const object_id_type& id )
    {
        auto object = get_object_record( id );
        if( NOT object.valid() )
            return oedge_record();
        FC_ASSERT( object->type() == edge_object, "This object is not an edge!"); // TODO check form ID as first check
        return object->as<edge_record>();
    }

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

      return fc::variant(string(price_to_pretty_print.ratio * obase_asset->precision / oquote_asset->precision)).as_double() / (BTS_BLOCKCHAIN_MAX_SHARES*1000);
   }

   string chain_interface::to_pretty_price( const price& price_to_pretty_print )const
   { try {
      auto obase_asset = get_asset_record( price_to_pretty_print.base_asset_id );
      if( !obase_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.base_asset_id) );

      auto oquote_asset = get_asset_record( price_to_pretty_print.quote_asset_id );
      if( !oquote_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.quote_asset_id) );

      auto tmp = price_to_pretty_print;
      tmp.ratio *= obase_asset->precision;
      tmp.ratio /= oquote_asset->precision;

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
         ugly_price.ratio *= quote_record->precision;
         ugly_price.ratio /= base_record->precision;
      }
      return ugly_price;
   } FC_CAPTURE_AND_RETHROW( (price_string)(base_symbol)(quote_symbol) ) }

   string chain_interface::to_pretty_asset( const asset& a )const
   {
      const auto oasset = get_asset_record( a.asset_id );
      const share_type amount = ( a.amount >= 0 ) ? a.amount : -a.amount;
      if( oasset.valid() )
      {
         const auto precision = oasset->precision;
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
