#pragma once 
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

   enum operation_type_enum
   {
      null_op_type              = 0,
      withdraw_op_type          = 1,
      deposit_op_type           = 2,
      reserve_name_op_type      = 3,
      update_name_op_type       = 4,
      create_asset_op_type      = 5,
      update_asset_op_type      = 6,
      issue_asset_op_type       = 7,
      fire_delegate_op_type     = 8,
      submit_proposal_op_type   = 9,
      vote_proposal_op_type     = 10
   };

   /**
    *  A poly-morphic operator that modifies the blockchain database 
    *  is some manner.
    */
   struct operation
   {
      operation():type(null_op_type){}

      operation( const operation& o )
      :type(o.type),data(o.data){}

      operation( operation&& o )
      :type(o.type),data(std::move(o.data)){}

      template<typename OperationType>
      operation( const OperationType& t )
      {
         type = OperationType::type;
         data = fc::raw::pack( t );
      }

      template<typename OperationType>
      OperationType as()const
      {
         FC_ASSERT( (operation_type_enum)type == OperationType::type, "", ("type",type)("OperationType",OperationType::type) );
         return fc::raw::unpack<OperationType>(data);
      }

      operation& operator=( const operation& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = o.data;
         return *this;
      }

      operation& operator=( operation&& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = std::move(o.data);
         return *this;
      }

      fc::enum_type<uint8_t,operation_type_enum> type;
      std::vector<char> data;
   };


   /** withdraws funds and moves them into the transaction
    * balance making them available for deposit
    */
   struct withdraw_operation
   {
       static const operation_type_enum type; 

       withdraw_operation():amount(0){}

       withdraw_operation( const balance_id_type& id, share_type amount_arg )
          :balance_id(id),amount(amount_arg){ FC_ASSERT( amount_arg > 0 ); }

       /** the account to withdraw from */
       balance_id_type    balance_id;
       /** that amount to withdraw from the account*/
       share_type         amount;
       /** any data required by the claim_condition */
       std::vector<char> claim_input_data;
   };


   /**
    *  The first time a deposit is made to a new address
    *  the condition under which it may be spent must be
    *  defined.  After the first deposit then future 
    *  deposits merely reference the address.
    */
   struct deposit_operation 
   {
       static const operation_type_enum type; 
       /** owner is just the hash of the condition */
       balance_id_type                balance_id()const;

       deposit_operation():amount(0){}
       deposit_operation( const address& owner, const asset& amnt, name_id_type delegate_id );

       /** the condition that the funds may be withdrawn,
        *  this is only necessary if the address is new.
        */
       share_type                       amount;
       withdraw_condition               condition;
   };

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

   struct reserve_name_operation
   {
      static const operation_type_enum type; 
      reserve_name_operation():is_delegate(false){}

      reserve_name_operation( const std::string& name, const fc::variant& json_data, const public_key_type& owner, const public_key_type& active, bool as_delegate = false );
      
      std::string         name;
      fc::variant         json_data;
      public_key_type     owner_key;
      public_key_type     active_key;
      bool                is_delegate;
   };

   struct update_name_operation
   {
      static const operation_type_enum type; 

      update_name_operation():name_id(0),is_delegate(false){}

      /** this should be 0 for creating a new name */
      name_id_type                  name_id;
      fc::optional<fc::variant>     json_data;
      fc::optional<public_key_type> active_key;
      bool                          is_delegate;
   };


   struct submit_proposal_operation
   {
      static const operation_type_enum type; 

      name_id_type          submitting_delegate_id; // the delegate_id of the submitter
      fc::time_point_sec    submission_date;
      std::string           subject;
      std::string           body;
      std::string           proposal_type; // alert, bug fix, feature upgrade, property change, etc
      fc::variant           data;  // data that is unique to the proposal
   };

   struct vote_proposal_operation
   {
      static const operation_type_enum type; 

      proposal_vote_id_type                            id;
      fc::time_point_sec                               timestamp;
      uint8_t                                          vote;
     // fc::enum_type<uint8_t,proposal_vote::vote_type>  vote;
   };

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::operation_type_enum,
                 (null_op_type)
                 (withdraw_op_type)
                 (deposit_op_type)
                 (create_asset_op_type)
                 (update_asset_op_type)
                 (reserve_name_op_type)
                 (update_name_op_type)
                 (issue_asset_op_type)
                 (submit_proposal_op_type)
                 (vote_proposal_op_type)
               )

FC_REFLECT( bts::blockchain::operation, (type)(data) )
FC_REFLECT( bts::blockchain::withdraw_operation, (balance_id)(amount)(claim_input_data) )
FC_REFLECT( bts::blockchain::deposit_operation, (amount)(condition) )
FC_REFLECT( bts::blockchain::create_asset_operation, (symbol)(name)(description)(json_data)(issuer_name_id)(maximum_share_supply) )
FC_REFLECT( bts::blockchain::update_asset_operation, (asset_id)(name)(description)(json_data)(issuer_name_id) )
FC_REFLECT( bts::blockchain::issue_asset_operation, (amount) )
FC_REFLECT( bts::blockchain::reserve_name_operation, (name)(json_data)(owner_key)(active_key)(is_delegate) )
FC_REFLECT( bts::blockchain::update_name_operation, (name_id)(json_data)(active_key)(is_delegate) )
FC_REFLECT( bts::blockchain::submit_proposal_operation, (submitting_delegate_id)(submission_date)(subject)(body)(proposal_type)(data) )
FC_REFLECT( bts::blockchain::vote_proposal_operation, (id)(timestamp)(vote) )

 
namespace fc {
   void to_variant( const bts::blockchain::operation& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::operation& vo );
}
