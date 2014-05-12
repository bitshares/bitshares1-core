#pragma once 
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {
   typedef fc::optional<signed_transaction> osigned_transaction;

   /**
    */
   struct account_record
   {
      account_record():balance(0){}
      account_record( const withdraw_condition& c )
      :balance(0),condition(c){}

      account_record( const address& owner, const asset& balance, name_id_type delegate_id );

      /** condition.get_address() */
      account_id_type            id()const { return condition.get_account(); }
      /** returns 0 if asset id is not condition.asset_id */
      asset                      get_balance( asset_id_type id )const;

      share_type                 balance;
      withdraw_condition         condition; 
      fc::time_point_sec         last_update;
   };
   typedef fc::optional<account_record> oaccount_record;

   struct asset_record
   {
      asset_record()
      :id(0),issuer_name_id(0),current_share_supply(0),maximum_share_supply(0),collected_fees(0){}

      share_type available_shares()const { return maximum_share_supply - current_share_supply; }

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      std::string         json_data;
      name_id_type        issuer_name_id;
      share_type          current_share_supply;
      share_type          maximum_share_supply;
      share_type          collected_fees;
   };
   typedef fc::optional<asset_record> oasset_record;

   struct name_record
   {
      name_record()
      :id(0),votes_for(0),votes_against(0),is_delegate(false),delegate_pay_balance(0){}

      static bool is_valid_name( const std::string& str );
      static bool is_valid_json( const std::string& str );
      int64_t net_votes()const { return votes_for - votes_against; }

      name_id_type        id;
      std::string         name;
      std::string         json_data;
      public_key_type     owner_key;
      public_key_type     active_key;
      share_type          votes_for;
      share_type          votes_against;
      bool                is_delegate;
      fc::time_point_sec  last_update;
      share_type          delegate_pay_balance;
   };
   typedef fc::optional<name_record> oname_record;


   class chain_interface
   {
      public:
         virtual ~chain_interface(){};
         /** return the timestamp from the most recent block */
         virtual fc::time_point_sec   timestamp()const = 0;

         /** return the current fee rate in millishares */
         virtual int64_t               get_fee_rate()const = 0;
         virtual int64_t               get_delegate_pay_rate()const = 0;
         virtual share_type            get_delegate_registration_fee()const;
         virtual share_type            get_asset_registration_fee()const;
                                       
         virtual oasset_record         get_asset_record( asset_id_type id )const                    = 0;
         virtual oaccount_record       get_account_record( const account_id_type& id )const         = 0;
         virtual oname_record          get_name_record( name_id_type id )const                      = 0;
         virtual otransaction_location get_transaction_location( const transaction_id_type& )const  = 0;
                                                                                                 
         virtual oasset_record         get_asset_record( const std::string& symbol )const          = 0;
         virtual oname_record          get_name_record( const std::string& name )const             = 0;
                                                                                                  
         virtual void                  store_asset_record( const asset_record& r )                 = 0;
         virtual void                  store_account_record( const account_record& r )             = 0;
         virtual void                  store_name_record( const name_record& r )                   = 0;
         virtual void                  store_transaction_location( const transaction_id_type&,  
                                                                   const transaction_location& loc ) = 0;
                                       
         virtual void                  apply_determinsitic_updates(){}
                                       
         virtual asset_id_type         last_asset_id()const = 0;
         virtual asset_id_type         new_asset_id() = 0;
                                       
         virtual name_id_type          last_name_id()const = 0;
         virtual name_id_type          new_name_id() = 0;
   };

   typedef std::shared_ptr<chain_interface> chain_interface_ptr;
} } // bts::blockchain 

FC_REFLECT( bts::blockchain::account_record, (balance)(condition)(last_update) )
FC_REFLECT( bts::blockchain::asset_record, (id)(symbol)(name)(description)(json_data)(issuer_name_id)(current_share_supply)(maximum_share_supply)(collected_fees) )
FC_REFLECT( bts::blockchain::name_record, 
            (id)(name)(json_data)(owner_key)(active_key)(votes_for)(votes_against)(is_delegate)(last_update)(delegate_pay_balance)
          )

