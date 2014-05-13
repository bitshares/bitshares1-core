#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   enum wallet_record_type
   {
      master_key_record_type     = 0,
      contact_record_type        = 1,
      transaction_record_type    = 2,
      name_record_type           = 3,
      asset_record_type          = 4,
      account_record_type        = 5,
      private_key_record_type    = 6,
      meta_record_type           = 7
   };

   struct wallet_record
   {
       wallet_record():type(0){}

       template<typename RecordType>
       wallet_record( const RecordType& rec )
       :type( RecordType::type ),json_data(rec)
       {
         // json_data = (rfc::json::to_string( rec );
       }

       template<typename RecordType>
       RecordType as()const;// { return json_data.as<RecordType>(); }

       fc::enum_type<uint8_t,wallet_record_type>   type;
       //std::string                                 json_data;
       fc::variant                                 json_data;
   };


   struct hkey_index
   {
      hkey_index( int32_t c = 0, int32_t t = 0, int32_t a = 0)
         :contact_num(c),trx_num(t),address_num(a){}

      int32_t contact_num;
      int32_t trx_num;
      int32_t address_num;
   };

   enum meta_record_property_enum
   {
      next_record_number,
      last_contact_index,
      default_transaction_fee,
      last_scanned_block_number
   };

   /** Used to store key/value property pairs.
    */
   struct wallet_meta_record
   {
       static const uint32_t type;

       wallet_meta_record( int32_t i = 0, 
                           meta_record_property_enum k = next_record_number, 
                           fc::variant v = fc::variant() )
       :index(i),key(k),value(v){}

       int32_t                                           index;
       fc::enum_type<int32_t, meta_record_property_enum> key;
       fc::variant                                       value;
   };

   /**
    *  Contacts are tracked by the hash of their receive key
    */
   struct wallet_contact_record
   {
       static const uint32_t type;
     
       int32_t               index;
       int32_t               contact_num;
       std::string           name;

       hkey_index get_next_receive_key_index( uint32_t trx_num = 0 );
       hkey_index get_next_send_key_index( uint32_t trx_num = 0 );

       /** helps facilitate key generation for transactions to/from this contact, 
        * this is provided by the remote user so we can send them transactions
        */
       extended_public_key                                                      extended_send_key;

       /** used for receiving funds form this contact, local wallet should have the private key*/
       extended_public_key                                                      extended_recv_key;

       std::map< uint32_t/*trx_num*/,uint32_t/*addr num*/ >                     last_send_hkey_index;
       std::map<uint32_t/*trx_num*/,uint32_t/*addr num*/ >                      last_recv_hkey_index;

       /** accounts with balances received from this contact sorted by asset_id */
       std::unordered_map< asset_id_type, std::unordered_set<account_id_type> > accounts;
       std::unordered_set<transaction_id_type>                                  transaction_history;
   };

   struct wallet_transaction_record
   {
       static const uint32_t type;
       wallet_transaction_record():transmit_count(0){}

       wallet_transaction_record( int32_t i, const signed_transaction& s )
       :index(i),trx(s){}

       int32_t                        index;
       signed_transaction             trx;
       std::string                    memo;
       fc::time_point                 received;
       transaction_location           location;
       /** the number of times this transaction has been transmitted */
       uint32_t                       transmit_count;
   };

   struct wallet_asset_record : public asset_record
   {
       static const uint32_t type;
       int32_t               index;

       wallet_asset_record():index(0){}
       wallet_asset_record( int32_t idx, const asset_record& rec )
       :asset_record( rec ), index( idx ){}
   };

   struct wallet_name_record : public name_record
   {
       static const uint32_t type;
       int32_t               index;

       wallet_name_record():index(0){}
       wallet_name_record( int32_t idx, const name_record& rec )
       :name_record( rec ), index( idx ){}

   };

   struct wallet_account_record : public account_record
   {
       static const uint32_t type;

       wallet_account_record( int32_t idx, const account_record& rec )
       :account_record( rec ), index( idx ){}

       wallet_account_record():index(0){}

       int32_t               index;
   };

   struct master_key_record 
   {
       static const uint32_t          type;
    
       int32_t                        index;
       extended_private_key  get_extended_private_key( const fc::sha512& password )const;
       std::vector<char>              encrypted_key;
       fc::sha512                     checksum;
   };

   /**
    *  Used to store extra private keys that are not derived from the hierarchial master
    *  key.
    */
   struct private_key_record
   {
       static const uint32_t          type;

       private_key_record():index(0),contact_index(0),extra_key_index(0){}
       private_key_record( int32_t index, 
                           int32_t contact_idx, 
                           int32_t extra_index,
                           const fc::ecc::private_key&, 
                           const fc::sha512& password );

       fc::ecc::private_key get_private_key( const fc::sha512& password )const;

       int32_t                        index;
       int32_t                        contact_index;
       int32_t                        extra_key_index;
       std::vector<char>              encrypted_key;
   };

} } // bts::wallet

FC_REFLECT_ENUM( bts::wallet::meta_record_property_enum,
                    (next_record_number)
                    (last_contact_index)
                    (default_transaction_fee)
                    (last_scanned_block_number)
                )

FC_REFLECT_ENUM( bts::wallet::wallet_record_type, 
                   (master_key_record_type)
                   (contact_record_type)
                   (transaction_record_type)
                   (account_record_type)
                   (name_record_type)
                   (private_key_record_type)
                   (asset_record_type)
                   (meta_record_type)
                )

FC_REFLECT( bts::wallet::wallet_meta_record, (index)(key)(value) )
FC_REFLECT( bts::wallet::wallet_record, (type)(json_data) )
FC_REFLECT( bts::wallet::hkey_index, (contact_num)(trx_num)(address_num) )
FC_REFLECT( bts::wallet::wallet_contact_record,
            (index)
            (contact_num)
            (name)
            (extended_send_key)
            (extended_recv_key)
            (last_send_hkey_index)
            (last_recv_hkey_index)
            (accounts)
            (transaction_history)
          )

FC_REFLECT( bts::wallet::wallet_transaction_record,
            (index)
            (trx)
            (memo)
            (received)
            (location) 
            (transmit_count)
          )

FC_REFLECT_DERIVED( bts::wallet::wallet_asset_record, (bts::wallet::asset_record), (index) )
FC_REFLECT_DERIVED( bts::wallet::wallet_account_record, (bts::wallet::account_record), (index) )
FC_REFLECT_DERIVED( bts::wallet::wallet_name_record, (bts::wallet::name_record), (index) )
FC_REFLECT( bts::wallet::master_key_record,  (index)(encrypted_key)(checksum) )
FC_REFLECT( bts::wallet::private_key_record,  (index)(contact_index)(extra_key_index)(encrypted_key) )

namespace bts { namespace wallet {
       template<typename RecordType>
       RecordType wallet_record::as()const
       {
          FC_ASSERT( (wallet_record_type)type == RecordType::type, "", 
                     ("type",type)
                     ("WithdrawType",RecordType::type) );
       
          return json_data.as<RecordType>();
          //fc::variant v = fc::json::from_string(json_data);
          //return v.as<RecordType>();
       }
} }
