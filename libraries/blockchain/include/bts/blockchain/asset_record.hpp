#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   struct asset_record
   {
      enum {
         market_issued_asset = -2
      };

      asset_record()
      :id(0),issuer_account_id(0),precision(0),current_share_supply(0),maximum_share_supply(0),collected_fees(0){}

      share_type available_shares()const { return maximum_share_supply - current_share_supply; }

      bool can_issue( asset amount )const
      {
         if( id != amount.asset_id ) return false;
         return can_issue( amount.amount );
      }
      bool can_issue( share_type amount )const
      { 
         if( amount <= 0 ) return false;
         auto new_share_supply = current_share_supply + amount;
         // catch overflow conditions
         return (new_share_supply > current_share_supply) && (new_share_supply < maximum_share_supply);
      }

      bool is_null()const            { return issuer_account_id == -1; }
      /** the asset is issued by the market and not by any user */
      bool is_market_issued()const   { return issuer_account_id == market_issued_asset; }
      asset_record make_null()const { asset_record cpy(*this); cpy.issuer_account_id = -1; return cpy; }

      uint64_t get_precision()const {  return precision ? precision : 1; }

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      fc::variant         public_data;
      account_id_type     issuer_account_id;
      uint64_t            precision;
      fc::time_point_sec  registration_date;
      fc::time_point_sec  last_update;
      share_type          current_share_supply;
      share_type          maximum_share_supply;
      share_type          collected_fees;
   };
   typedef fc::optional<asset_record> oasset_record;

} } // bts::blockchain 

FC_REFLECT( bts::blockchain::asset_record, (id)(symbol)(name)(description)(public_data)(issuer_account_id)(precision)(current_share_supply)(maximum_share_supply)(collected_fees)(registration_date) )

