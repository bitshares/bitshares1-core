#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/config.hpp>
#include <sstream>

namespace bts{ namespace blockchain {
   balance_record::balance_record( const address& owner, const asset& balance_arg, name_id_type delegate_id )
   {
      balance =  balance_arg.amount;
      condition = withdraw_condition( withdraw_with_signature( owner ), balance_arg.asset_id, delegate_id );
   }
   /** returns 0 if asset id is not condition.asset_id */
   asset     balance_record::get_balance( asset_id_type id )const
   {
      if( id != condition.asset_id ) return asset( 0, id );
      return asset( balance, id );
   }

   share_type     chain_interface::get_delegate_registration_fee()const
   {
      return (get_fee_rate() * BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE)/1000;
   }
   share_type    chain_interface::get_asset_registration_fee()const
   {
      return (get_fee_rate() * BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE)/1000;
   }

   bool name_record::is_valid_name( const std::string& str )
   {
      if( str.size() == 0 ) return false;
      if( str.size() > BTS_BLOCKCHAIN_MAX_NAME_SIZE ) return false;
      if( str[0] < 'a' || str[0] > 'z' ) return false;
      for( auto c : str )
      {
          if( c >= 'a' && c <= 'z' ) continue;
          else if( c >= '0' && c <= '9' ) continue;
          else if( c == '-' ) continue;
          else return false;
      }
      return true;
   }
   bool name_record::is_valid_json( const std::string& str )
   {
      std::stringstream ss(str);
      // TODO: validate this!
      return true;
   }

} }  // bts::blockchain

