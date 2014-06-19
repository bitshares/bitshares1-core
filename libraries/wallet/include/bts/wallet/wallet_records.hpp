#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   enum wallet_record_type_enum
   {
      master_key_record_type     = 0,
      account_record_type        = 1,
      key_record_type            = 2,
      transaction_record_type    = 3,
      asset_record_type          = 5,
      balance_record_type        = 6,
      property_record_type       = 7,
      market_order_record_type   = 8,
      setting_record_type        = 9
   };

   struct generic_wallet_record
   {
       generic_wallet_record():type(0){}

       template<typename RecordType>
       generic_wallet_record( const RecordType& rec )
       :type( int(RecordType::type) ),data(rec)
       { }

       template<typename RecordType>
       RecordType as()const;

       int32_t get_wallet_record_index()const 
       { try {
          FC_ASSERT( data.is_object() );
          FC_ASSERT( data.get_object().contains( "index" ) );
          return data.get_object()["index"].as<int32_t>();
       } FC_RETHROW_EXCEPTIONS( warn, "" ) }

       fc::enum_type<uint8_t,wallet_record_type_enum>   type;
       fc::variant                                      data;
   };

   template<wallet_record_type_enum RecordType>
   struct base_record 
   {
      enum { type  = RecordType };

      base_record( int32_t idx = 0 ):wallet_record_index(idx){}
      int32_t wallet_record_index;
   };

   enum property_enum
   {
      next_record_number,
      default_transaction_priority_fee,
      next_child_key_index,
      last_locked_scanned_block_number,
      last_unlocked_scanned_block_number
   };

   /** Used to store key/value property pairs.
    */
   struct wallet_property
   {
       wallet_property( property_enum k = next_record_number, 
                        fc::variant v = fc::variant() )
       :key(k),value(v){}

       fc::enum_type<int32_t, property_enum> key;
       fc::variant                           value;
   };

   /**
    *  Contacts are tracked by the hash of their receive key
    */
   struct account : public bts::blockchain::account_record
   {
       account()
       :trust_level(0)
       ,block_production_enabled(false)
       ,is_my_account(false)
       {}

       address           account_address;
       /**
        * Data kept locally for this account
        */
       variant           private_data;

       /** 
        * If registered account id is a delegate ID then 
        * this trust level indicates whether the user wants to
        * vote for, against, or neutral for a given delegate.
        *
        * The assumption is that if the delegate is in the
        * users wallet then they are a potential canidate.
        */
       int32_t           trust_level;

       bool              block_production_enabled;
       bool              is_my_account;
   };


   template<typename RecordTypeName, wallet_record_type_enum RecordTypeNumber>
   struct wallet_record : public base_record<RecordTypeNumber>, public RecordTypeName
   {
      wallet_record(){}
      wallet_record( const RecordTypeName& rec, int32_t wallet_record_index = 0 )
      :base_record<RecordTypeNumber>(wallet_record_index),RecordTypeName(rec){}
   };


   struct master_key
   {
       std::vector<char>              encrypted_key;
       fc::sha512                     checksum;

       bool                           validate_password( const fc::sha512& password )const;
       extended_private_key           decrypt_key( const fc::sha512& password )const;
       void                           encrypt_key( const fc::sha512& password, 
                                                   const extended_private_key& k );
   };

   struct key_data
   {
       key_data():valid_from_signature(false){}

       address                  account_address;
       public_key_type          public_key;
       std::vector<char>        encrypted_private_key;
       bool                     valid_from_signature;
       optional<string>         memo;

       address                  get_address()const { return address( public_key ); }
       bool                     has_private_key()const;
       void                     encrypt_private_key( const fc::sha512& password, 
                                                     const fc::ecc::private_key& );
       fc::ecc::private_key     decrypt_private_key( const fc::sha512& password )const;
   };

   struct transaction_data
   {
       transaction_data():fees(0),block_num(0),transmit_count(0){}
       transaction_data( const signed_transaction& t ):trx(t),fees(0),block_num(0),transmit_count(0){}

       signed_transaction        trx;
       transaction_id_type       transaction_id;
       optional<public_key_type> to_account;
       optional<public_key_type> from_account;
       asset                     amount;
       share_type                fees;
       std::string               memo_message;
       uint32_t                  block_num;
       fc::time_point            created_time;
       fc::time_point            received_time;
       /** the number of times this transaction has been transmitted */
       uint32_t                  transmit_count;
       vector<address>           extra_addresses;
   };


   struct market_order_status
   {
      market_order_status():proceeds(0){}

      order_type_enum get_type()const;
      string          get_id()const;

      asset           get_balance()const; // funds available for this order
      asset           get_proceeds()const;
      asset           get_quantity()const;
      price           get_price()const;

      bts::blockchain::market_order        order;
      share_type                           proceeds;
      unordered_set<transaction_id_type>   transactions;
   };

   /* Used to store GUI preferences and such */
   struct setting
   {
      setting(){};
      setting(string name, variant value):name(name),value(value){};
      string       name;
      variant      value;
   };

   /** cached blockchain data */
   typedef wallet_record< bts::blockchain::asset_record,   asset_record_type       >  wallet_asset_record;
   typedef wallet_record< bts::blockchain::balance_record, balance_record_type     >  wallet_balance_record;

   /** records unique to the wallet */
   typedef wallet_record< transaction_data,                transaction_record_type >  wallet_transaction_record;
   typedef wallet_record< master_key,                      master_key_record_type  >  wallet_master_key_record;
   typedef wallet_record< key_data,                        key_record_type         >  wallet_key_record;
   typedef wallet_record< account,                         account_record_type     >  wallet_account_record;
   typedef wallet_record< wallet_property,                 property_record_type    >  wallet_property_record;
   typedef wallet_record< market_order_status,             market_order_record_type>  wallet_market_order_status_record;
   typedef wallet_record< setting,                         setting_record_type     >  wallet_setting_record;

   typedef optional< wallet_transaction_record >            owallet_transaction_record;
   typedef optional< wallet_master_key_record >             owallet_master_key_record;
   typedef optional< wallet_key_record >                    owallet_key_record;
   typedef optional< wallet_account_record >                owallet_account_record;
   typedef optional< wallet_property_record >               owallet_property_record;
   typedef optional< wallet_balance_record >                owallet_balance_record;
   typedef optional< wallet_market_order_status_record >    owallet_market_order_record;
   typedef optional< wallet_setting_record >                owallet_setting_record;

} } // bts::wallet

FC_REFLECT_ENUM( bts::wallet::property_enum,
                    (next_record_number)
                    (default_transaction_priority_fee)
                    (next_child_key_index)
                    (last_locked_scanned_block_number)
                    (last_unlocked_scanned_block_number)
               )

FC_REFLECT_ENUM( bts::wallet::wallet_record_type_enum, 
                   (master_key_record_type)
                   (key_record_type)
                   (account_record_type)
                   (transaction_record_type)
                   (balance_record_type)
                   (asset_record_type)
                   (property_record_type)
                   (market_order_record_type)
                   (setting_record_type)
                )

FC_REFLECT( bts::wallet::wallet_property, (key)(value) )
FC_REFLECT( bts::wallet::generic_wallet_record, (type)(data) )
FC_REFLECT( bts::wallet::master_key, (encrypted_key)(checksum) )
FC_REFLECT( bts::wallet::key_data, (account_address)(public_key)(encrypted_private_key)(memo) )
FC_REFLECT( bts::wallet::transaction_data, 
            (trx)
            (transaction_id)
            (to_account)
            (from_account)
            (amount)
            (fees)
            (memo_message)
            (created_time)
            (received_time)
            (block_num)
            (transmit_count) )
FC_REFLECT_DERIVED( bts::wallet::account, (bts::blockchain::account_record), (account_address)(trust_level)(block_production_enabled)(private_data)(is_my_account) )

FC_REFLECT( bts::wallet::market_order_status, (order)(proceeds)(transactions) )
FC_REFLECT( bts::wallet::setting, (name)(value) )



/**
 *  Implement generic reflection for wallet record types
 */
namespace fc {

   template<typename T, bts::wallet::wallet_record_type_enum N> 
   struct get_typename< bts::wallet::wallet_record<T,N> > 
   { 
      static const char* name() 
      { 
         static std::string _name =  get_typename<T>::name() + std::string("_record"); 
         return _name.c_str();
      }
   };

   template<typename Type, bts::wallet::wallet_record_type_enum N> 
   struct reflector<bts::wallet::wallet_record<Type,N>> 
   {
      typedef bts::wallet::wallet_record<Type,N>  type;
      typedef fc::true_type is_defined;
      typedef fc::false_type is_enum; 
      enum member_count_enum {
         local_member_count = 1,
         total_member_count = local_member_count + reflector<Type>::total_member_count
      };

      template<typename Visitor> 
      static void visit( const Visitor& visitor )
      {
          { 
            typedef decltype(((bts::wallet::base_record<N>*)nullptr)->wallet_record_index) member_type;  
            visitor.TEMPLATE operator()<member_type,bts::wallet::base_record<N>,&bts::wallet::base_record<N>::wallet_record_index>( "index" ); 
          }
          
          fc::reflector<Type>::visit( visitor );
      }
   };
} // namespace fc

namespace bts { namespace wallet {
       template<typename RecordType>
       RecordType generic_wallet_record::as()const
       {
          FC_ASSERT( (wallet_record_type_enum)type == RecordType::type, "", 
                     ("type",type)
                     ("WithdrawType",(wallet_record_type_enum)RecordType::type) );
       
          return data.as<RecordType>();
       }
} }
