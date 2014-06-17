#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <fc/io/json.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace bts{ namespace blockchain {
   balance_record::balance_record( const address& owner, const asset& balance_arg, account_id_type delegate_id )
   {
      balance =  balance_arg.amount;
      condition = withdraw_condition( withdraw_with_signature( owner ), balance_arg.asset_id, delegate_id );
   }

   /** returns 0 if asset id is not condition.asset_id */
   asset     balance_record::get_balance()const
   {
      return asset( balance, condition.asset_id );
   }

   address balance_record::owner()const
   {
      if( condition.type == withdraw_signature_type )
         return condition.as<withdraw_with_signature>().owner;
      return address();
   }

   share_type     chain_interface::get_delegate_registration_fee()const
   {
      return (get_fee_rate() * BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE)/1000;
   }
   share_type    chain_interface::get_asset_registration_fee()const
   {
      return (get_fee_rate() * BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE)/1000;
   }
   
   share_type    chain_interface::calculate_data_fee(size_t bytes) const
   {
      return (get_fee_rate() * bytes)/1000;
   }

   bool chain_interface::is_valid_account_name( const std::string& str )const
   {
      if( str.size() < BTS_BLOCKCHAIN_MIN_NAME_SIZE ) return false;
      if( str.size() > BTS_BLOCKCHAIN_MAX_NAME_SIZE ) return false;
      if( str[0] < 'a' || str[0] > 'z' ) return false;
      for( auto c : str )
      {
          if( c >= 'a' && c <= 'z' ) continue;
          else if( c >= '0' && c <= '9' ) continue;
          else if( c == '-' ) continue;
          else if( c == '.' ) continue;
          else return false;
      }
      return true;
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

   std::vector<account_id_type> chain_interface::get_active_delegates()const
   { try {
      return get_property( active_delegate_list_id ).as<std::vector<account_id_type> >();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void                      chain_interface::set_active_delegates( const std::vector<account_id_type>& delegate_ids )
   {
      set_property( active_delegate_list_id, fc::variant(delegate_ids) );
   }

   bool                      chain_interface::is_active_delegate( account_id_type delegate_id ) const
   { try {
      auto active = get_active_delegates();
      return active.end() != std::find( active.begin(), active.end(), delegate_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("delegate_id",delegate_id) ) }


   string  chain_interface::to_pretty_price( const price& price_to_pretty_print )const
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


   string  chain_interface::to_pretty_asset( const asset& a )const
   {
      auto oasset = get_asset_record( a.asset_id );
      if( oasset )
      {
         string decimal = fc::to_string(oasset->get_precision() + a.amount%oasset->get_precision());
         decimal[0] = '.';
         return fc::to_pretty_string( a.amount / oasset->get_precision()) +  decimal + " " + oasset->symbol;
      }
      else
         return fc::to_pretty_string( a.amount ) + " ???";
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
      for( auto c : name )
         if( !std::isalnum(c) || !std::isupper(c) )
            return false;
      return true;
   }

} }  // bts::blockchain

