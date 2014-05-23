#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain { 

   /**
    *  Creates / defines an asset type but does not
    *  allocate it to anyone.  Use issue_asset_opperation
    */
   struct create_asset_operation
   {
       static const operation_type_enum type; 

       /**
        * Symbols may only contain A-Z and 0-9 and up to 5
        * characters and must be unique.
        */
       std::string         symbol;

       /**
        * Names are a more complete description and may
        * contain any kind of characters or spaces.
        */
       std::string         name;
       /**
        *  Descripts the asset and its purpose.
        */
       std::string         description;
       /**
        * Other information relevant to this asset.
        */
       fc::variant         json_data;

       /**
        *  Assets can only be issued by individuals that
        *  have registered a name.
        */
       name_id_type        issuer_name_id;

       /** The maximum number of shares that may be allocated */
       share_type          maximum_share_supply;
   };

   /**
    * This operation updates an existing issuer record provided
    * it is signed by a proper key.
    */
   struct update_asset_operation
   {
       static const operation_type_enum type; 

       asset_id_type        asset_id;
       std::string          name;
       std::string          description;
       fc::variant          json_data;
       name_id_type         issuer_name_id;
   };

   /**
    *  Transaction must be signed by the active key
    *  on the issuer_name_record.
    *
    *  The resulting amount of shares must be below
    *  the maximum share supply.
    */
   struct issue_asset_operation
   {
       static const operation_type_enum type; 

       issue_asset_operation( asset a = asset() ):amount(a){}
       asset            amount;
   };

} } // bts::blockchain 

FC_REFLECT( bts::blockchain::create_asset_operation, (symbol)(name)(description)(json_data)(issuer_name_id)(maximum_share_supply) )
FC_REFLECT( bts::blockchain::update_asset_operation, (asset_id)(name)(description)(json_data)(issuer_name_id) )
FC_REFLECT( bts::blockchain::issue_asset_operation, (amount) )
