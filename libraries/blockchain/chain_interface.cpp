#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

#include <algorithm>
#include <locale>

namespace bts{ namespace blockchain {

   balance_record::balance_record( const address& owner, const asset& balance_arg, slate_id_type delegate_id )
   {
      balance =  balance_arg.amount;
      condition = withdraw_condition( withdraw_with_signature( owner ), balance_arg.asset_id, delegate_id );
   }

   /** returns 0 if asset id is not condition.asset_id */
   asset balance_record::get_balance()const
   {
      return asset( balance, condition.asset_id );
   }

   address balance_record::owner()const
   {
      if( condition.type == withdraw_signature_type )
         return condition.as<withdraw_with_signature>().owner;
      return address();
   }

   share_type chain_interface::get_delegate_registration_fee()const
   {
      return (get_delegate_pay_rate() * BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE)/BTS_BLOCKCHAIN_NUM_DELEGATES;
   }

   share_type chain_interface::get_asset_registration_fee()const
   {
      return (get_delegate_pay_rate() * BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE);
   }
   
   share_type chain_interface::calculate_data_fee(size_t bytes) const
   {
      return (get_fee_rate() * bytes)/1000;
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

   asset_id_type chain_interface::last_asset_id()const
   {
       return get_property( chain_property_enum::last_asset_id ).as<asset_id_type>();
   }

   asset_id_type  chain_interface::new_asset_id()
   {
      auto next_id = last_asset_id() + 1;
      set_property( chain_property_enum::last_asset_id, next_id );
      return next_id;
   }

   account_id_type   chain_interface::last_account_id()const
   {
       return get_property( chain_property_enum::last_account_id ).as<account_id_type>();
   }

   account_id_type   chain_interface::new_account_id()
   {
      auto next_id = last_account_id() + 1;
      set_property( chain_property_enum::last_account_id, next_id );
      return next_id;
   }

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

   vector<account_id_type> chain_interface::get_active_delegates()const
   { try {
      return get_property( active_delegate_list_id ).as<std::vector<account_id_type>>();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_interface::set_active_delegates( const std::vector<account_id_type>& delegate_ids )
   {
      set_property( active_delegate_list_id, fc::variant(delegate_ids) );
   }

   bool chain_interface::is_active_delegate( account_id_type delegate_id ) const
   { try {
      auto active = get_active_delegates();
      return active.end() != std::find( active.begin(), active.end(), delegate_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("delegate_id",delegate_id) ) }

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

   int64_t   chain_interface::get_required_confirmations()const
   {
      return get_property( confirmation_requirement ).as_int64(); 
   }

   bool chain_interface::is_valid_symbol_name( const string& name )const
   {
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

   share_type  chain_interface::get_delegate_pay_rate()const
   {
      return get_accumulated_fees() / (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*14);
   }

   share_type  chain_interface::get_accumulated_fees()const
   {
      return get_property( accumulated_fees ).as_int64();
   }

   void  chain_interface::set_accumulated_fees( share_type fees )
   {
      set_property( accumulated_fees, variant(fees) );
   }
   share_type  chain_interface::get_fee_rate()const
   {
      return get_property( current_fee_rate ).as_int64();
   }

   void  chain_interface::set_fee_rate( share_type fees )
   {
      set_property( current_fee_rate, variant(fees) );
   }

   map<asset_id_type, asset_id_type>  chain_interface::get_dirty_markets()const
   {
      try{
         return get_property( dirty_markets ).as<map<asset_id_type,asset_id_type> >();
      } catch ( ... )
      {
         return map<asset_id_type,asset_id_type>();
      }
   }
   void  chain_interface::set_dirty_markets( const map<asset_id_type,asset_id_type>& d )
   {
      set_property( dirty_markets, fc::variant(d) );
   }

} } // bts::blockchain
