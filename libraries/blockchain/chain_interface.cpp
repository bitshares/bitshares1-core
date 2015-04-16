#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <fc/io/raw_variant.hpp>

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

   bool chain_interface::is_valid_account_name( const string& name )
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
    * Symbol names can be hierarchical: for example a primary symbol name, a '.', and a sub-symbol name.
    * A primary symbol name must be a minimum of 3 and a maximum of 8 characters in length.
    * Primary names can only contain uppercase letters (digits are not allowed to avoid 0 and 1 spoofing).
    * A hierarchical symbol name (consisting of a primary symbol name, a dot, and a sub-symbol name) can be up to 12 chars
    * in total length (including the dot).
    * Sub-symbol names can contain uppercase letters or digits (digits are allowed in sub-symbols because the namespace is
    * overseen by the owner of the primary symbol and is therefore not subject to spoofing).
    *
    * To fit under the 12 character limit, it is likely that users planning to register hierarchical names will
    * choose shorter (more expensive) symbol names for their primary symbol, so that they can mirror more primary symbol names.
    * The max of 12 for hierarchical symbol names will allow hierarchical mirroring of long primary symbol characters
    * as long as the primary symbol buyer purchases a symbol of 3 in size. For example, if CRY was chosen as the primary symbol,
    * CRY.ABCDEFGH could be registered. But if a longer name was chosen as a primary symbol, such as CRYPTO,
    * then only symbols up to 5 in length can be mirrored (i.e CRYPTO.ABCDEFGH would be too long).
    */
   bool chain_interface::is_valid_symbol_name( const string& symbol )const
   { try {
       if( get_head_block_num() < BTS_V0_5_0_FORK_BLOCK_NUM )
           return is_valid_symbol_name_v1( symbol );

       FC_ASSERT( symbol != "BTSX" );

       if( symbol.size() < BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE)
         FC_ASSERT(false, "Symbol name too small");

       int dots = 0;
       int dot_position = 0;
       int position = 0;
       for( const char c : symbol )
       {
          if( c == '.' ) //if we have hierarchical name
          {
            dot_position =  position;
            if ( ++dots > 1 )
              FC_ASSERT(false, "Symbol names can have at most one dot");
          }
          else if (dots == 0 && !std::isupper( c, std::locale::classic() ) )
              FC_ASSERT(false, "Primary symbol names can only contain uppercase letters");
          else if (!std::isupper( c, std::locale::classic() ) &&
                  !std::isdigit( c, std::locale::classic() )     )
            FC_ASSERT(false, "Sub-symbol names can only contain uppercase letters or digits");
          ++position;
       }

       if( symbol.back() == '.' ) return false;
       if( symbol.front() == '.' ) return false;

       if (dots == 0)
       {
         if (position > BTS_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE)
           FC_ASSERT(false, "Symbol name too large");
       }
       else //dots == 1 means hierarchial asset name
       {
         if (position - dot_position - 1> BTS_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE)
           FC_ASSERT(false, "Sub-symbol name too large");
         if( symbol.size() > BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE)
           FC_ASSERT(false, "Symbol name too large");
       }

       if( symbol.find( "BIT" ) == 0 )
         FC_ASSERT(false, "Symbol names cannot be prefixed with BIT");

       return true;
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   time_point_sec chain_interface::get_genesis_timestamp()const
   {
       return get_asset_record( asset_id_type() )->registration_date;
   }

   // Starting at genesis, delegates are issued max 50 shares per block produced, and this value is halved every 4 years
   // just like in Bitcoin
   share_type chain_interface::get_max_delegate_pay_issued_per_block()const
   {
       share_type pay_per_block = BTS_MAX_DELEGATE_PAY_PER_BLOCK;

       static const time_point_sec start_timestamp = get_genesis_timestamp();
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
       if( get_head_block_num() < BTS_V0_4_24_FORK_BLOCK_NUM )
           return get_delegate_registration_fee_v1( pay_rate );

       if( pay_rate == 0 ) return 0;
       static const uint32_t blocks_per_two_weeks = 14 * BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
       const share_type max_total_pay_per_two_weeks = blocks_per_two_weeks * get_max_delegate_pay_issued_per_block();
       const share_type max_pay_per_two_weeks = max_total_pay_per_two_weeks / BTS_BLOCKCHAIN_NUM_DELEGATES;
       const share_type registration_fee = (max_pay_per_two_weeks * pay_rate) / 100;
       FC_ASSERT( registration_fee > 0 );
       return registration_fee;
   }

   share_type chain_interface::get_asset_registration_fee( uint8_t symbol_length )const
   {
       if( get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
           return get_asset_registration_fee_v2( symbol_length );

       // TODO: Add #define's for these fixed prices
       static const share_type long_symbol_price = 5000 * BTS_BLOCKCHAIN_PRECISION; // $100 at $0.02/share
       static const share_type short_symbol_price = 100 * long_symbol_price; // $10,000 at same price
       FC_ASSERT( long_symbol_price > 0 );
       FC_ASSERT( short_symbol_price > long_symbol_price );
       return symbol_length <= 5 ? short_symbol_price : long_symbol_price;
   }

   asset_id_type chain_interface::last_asset_id()const
   { try {
       const oproperty_record record = get_property_record( property_id_type::last_asset_id );
       FC_ASSERT( record.valid() );
       return record->value.as<asset_id_type>();
   } FC_CAPTURE_AND_RETHROW() }

   asset_id_type chain_interface::new_asset_id()
   { try {
       const asset_id_type next_id = last_asset_id() + 1;
       store_property_record( property_id_type::last_asset_id, variant( next_id ) );
       return next_id;
   } FC_CAPTURE_AND_RETHROW() }

   account_id_type chain_interface::last_account_id()const
   { try {
       const oproperty_record record = get_property_record( property_id_type::last_account_id );
       FC_ASSERT( record.valid() );
       return record->value.as<account_id_type>();
   } FC_CAPTURE_AND_RETHROW() }

   account_id_type chain_interface::new_account_id()
   { try {
       const account_id_type next_id = last_account_id() + 1;
       store_property_record( property_id_type::last_account_id, variant( next_id ) );
       return next_id;
   } FC_CAPTURE_AND_RETHROW() }

   vector<account_id_type> chain_interface::get_active_delegates()const
   { try {
       const oproperty_record record = get_property_record( property_id_type::active_delegate_list_id );
       FC_ASSERT( record.valid() );
       return record->value.as<vector<account_id_type>>();
   } FC_CAPTURE_AND_RETHROW() }

   void chain_interface::set_active_delegates( const std::vector<account_id_type>& active_delegates )
   { try {
       store_property_record( property_id_type::active_delegate_list_id, variant( active_delegates ) );
   } FC_CAPTURE_AND_RETHROW( (active_delegates) ) }

   bool chain_interface::is_active_delegate( const account_id_type id )const
   { try {
       const auto active_delegates = get_active_delegates();
       return std::count( active_delegates.begin(), active_delegates.end(), id ) > 0;
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   string chain_interface::to_pretty_price( const price& price_to_pretty_print, const bool include_units )const
   { try {
      auto obase_asset = get_asset_record( price_to_pretty_print.base_asset_id );
      if( !obase_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.base_asset_id) );

      auto oquote_asset = get_asset_record( price_to_pretty_print.quote_asset_id );
      if( !oquote_asset ) FC_CAPTURE_AND_THROW( unknown_asset_id, (price_to_pretty_print.quote_asset_id) );

      auto tmp = price_to_pretty_print;
      tmp.ratio *= obase_asset->precision;
      tmp.ratio /= oquote_asset->precision;

      string pretty_price = tmp.ratio_string();

      if( include_units )
          pretty_price += " " + oquote_asset->symbol + " / " + obase_asset->symbol;

      return pretty_price;
   } FC_CAPTURE_AND_RETHROW( (price_to_pretty_print)(include_units) ) }

   asset chain_interface::to_ugly_asset(const std::string& amount, const std::string& symbol) const
   { try {
      const auto record = get_asset_record( symbol );
      if( !record ) FC_CAPTURE_AND_THROW( unknown_asset_symbol, (symbol) );
      return record->asset_from_string(amount);
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
   } FC_CAPTURE_AND_RETHROW( (price_string)(base_symbol)(quote_symbol)(do_precision_dance) ) }

   string chain_interface::to_pretty_asset( const asset& a )const
   { try {
      const auto oasset = get_asset_record( a.asset_id );
      if( oasset.valid() ) return oasset->amount_to_string(a.amount);
      return fc::to_pretty_string( a.amount ) + " ???";
   } FC_CAPTURE_AND_RETHROW( (a) ) }

   void chain_interface::set_chain_id( const digest_type& id )
   { try {
       store_property_record( property_id_type::chain_id, variant( id ) );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   digest_type chain_interface::get_chain_id()const
   { try {
       static optional<digest_type> value;
       if( value.valid() ) return *value;
       const oproperty_record record = get_property_record( property_id_type::chain_id );
       FC_ASSERT( record.valid() );
       value = record->value.as<digest_type>();
       return *value;
   } FC_CAPTURE_AND_RETHROW() }

   fc::ripemd160 chain_interface::get_current_random_seed()const
   { try {
       const oproperty_record record = get_property_record( property_id_type::last_random_seed_id );
       if( record.valid() ) return record->value.as<fc::ripemd160>();
       return fc::ripemd160();
   } FC_CAPTURE_AND_RETHROW() }

   void chain_interface::set_statistics_enabled( const bool enabled )
   { try {
       store_property_record( property_id_type::statistics_enabled, variant( enabled ) );
   } FC_CAPTURE_AND_RETHROW( (enabled) ) }

   bool chain_interface::get_statistics_enabled()const
   { try {
       static optional<bool> value;
       if( value.valid() ) return *value;
       const oproperty_record record = get_property_record( property_id_type::statistics_enabled );
       FC_ASSERT( record.valid() );
       value = record->value.as<bool>();
       return *value;
   } FC_CAPTURE_AND_RETHROW() }

   void chain_interface::set_dirty_markets( const std::set<std::pair<asset_id_type, asset_id_type>>& markets )
   { try {
       store_property_record( property_id_type::dirty_markets, variant( markets ) );
   } FC_CAPTURE_AND_RETHROW( (markets) ) }

   std::set<std::pair<asset_id_type, asset_id_type>> chain_interface::get_dirty_markets()const
   { try {
       const oproperty_record record = get_property_record( property_id_type::dirty_markets );
       if( record.valid() ) return record->value.as<std::set<std::pair<asset_id_type, asset_id_type>>>();
       return std::set<std::pair<asset_id_type, asset_id_type>>();
   } FC_CAPTURE_AND_RETHROW() }

   oproperty_record chain_interface::get_property_record( const property_id_type id )const
   { try {
       return lookup<property_record>( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   void chain_interface::store_property_record( const property_id_type id, const variant& value)
   { try {
       store( id, property_record{ id, value } );
   } FC_CAPTURE_AND_RETHROW( (id)(value) ) }

   oaccount_record chain_interface::get_account_record( const account_id_type id )const
   { try {
       return lookup<account_record>( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   oaccount_record chain_interface::get_account_record( const string& name )const
   { try {
       return lookup<account_record>( name );
   } FC_CAPTURE_AND_RETHROW( (name) ) }

   oaccount_record chain_interface::get_account_record( const address& addr )const
   { try {
       return lookup<account_record>( addr );
   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   void chain_interface::store_account_record( const account_record& record )
   { try {
       store( record.id, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   oasset_record chain_interface::get_asset_record( const asset_id_type id )const
   { try {
       return lookup<asset_record>( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   oasset_record chain_interface::get_asset_record( const string& symbol )const
   { try {
       return lookup<asset_record>( symbol );
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   void chain_interface::store_asset_record( const asset_record& record )
   { try {
       store( record.id, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   oslate_record chain_interface::get_slate_record( const slate_id_type id )const
   { try {
       return lookup<slate_record>( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   void chain_interface::store_slate_record( const slate_record& record )
   { try {
       store( record.id(), record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   obalance_record chain_interface::get_balance_record( const balance_id_type& id )const
   { try {
       return lookup<balance_record>( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   void chain_interface::store_balance_record( const balance_record& record )
   { try {
       store( record.id(), record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   oburn_record chain_interface::get_burn_record( const burn_index& index )const
   { try {
       return lookup<burn_record>( index );
   } FC_CAPTURE_AND_RETHROW( (index) ) }

   void chain_interface::store_burn_record( const burn_record& record )
   { try {
       store( record.index, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   ostatus_record chain_interface::get_status_record( const status_index index )const
   { try {
       return lookup<status_record>( index );
   } FC_CAPTURE_AND_RETHROW( (index) ) }

   void chain_interface::store_status_record( const status_record& record )
   { try {
       store( record.index, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   ofeed_record chain_interface::get_feed_record( const feed_index index )const
   { try {
       return lookup<feed_record>( index );
   } FC_CAPTURE_AND_RETHROW( (index) ) }

   void chain_interface::store_feed_record( const feed_record& record )
   { try {
       store( record.index, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   oslot_record chain_interface::get_slot_record( const slot_index index )const
   { try {
       return lookup<slot_record>( index );
   } FC_CAPTURE_AND_RETHROW( (index) ) }

   oslot_record chain_interface::get_slot_record( const time_point_sec timestamp )const
   { try {
       return lookup<slot_record>( timestamp );
   } FC_CAPTURE_AND_RETHROW( (timestamp) ) }

   void chain_interface::store_slot_record( const slot_record& record )
   { try {
       store( record.index, record );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

} } // bts::blockchain
