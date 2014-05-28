#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   typedef std::vector<fc::ecc::private_key> private_keys;

   enum wallet_record_type_enum
   {
      master_key_record_type     = 0,
      account_record_type        = 1,
      key_record_type            = 2,
      transaction_record_type    = 3,
      name_record_type           = 4,
      asset_record_type          = 5,
      balance_record_type        = 6,
      property_record_type       = 7,
      identity_record_type       = 8,
      memo_record_type           = 9
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

       int32_t get_index()const 
       { try {
          FC_ASSERT( data.is_object() );
          FC_ASSERT( data.get_object().contains( "index" ) );
          return data.get_object()["index"].as_int64();
       } FC_RETHROW_EXCEPTIONS( warn, "" ) }

       fc::enum_type<uint8_t,wallet_record_type_enum>   type;
       fc::variant                                      data;
   };

   template<wallet_record_type_enum RecordType>
   struct base_record 
   {
      enum { type  = RecordType };

      base_record( int32_t idx = 0 ):index(idx){}
      int32_t index;
   };

   enum property_enum
   {
      next_record_number,
      default_transaction_fee,
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
   struct account
   {
       std::string    name;
       name_id_type   registered_name_id;  
       address        account_address;
   };


   template<typename RecordTypeName, wallet_record_type_enum RecordTypeNumber>
   struct wallet_record : public base_record<RecordTypeNumber>, public RecordTypeName
   {
      wallet_record(){}
      wallet_record( const RecordTypeName& rec, int32_t index = 0 )
      :base_record<RecordTypeNumber>(index),RecordTypeName(rec){}
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
       key_data(){}

       address                  account_address;
       public_key_type          public_key;
       std::vector<char>        encrypted_private_key;
       bool                     valid_from_signature;
       omemo_data               memo;

       address                  get_address()const { return address( public_key ); }
       bool                     has_private_key()const;
       void                     encrypt_private_key( const fc::sha512& password, 
                                                     const fc::ecc::private_key& );
       fc::ecc::private_key     decrypt_private_key( const fc::sha512& password )const;
   };

   struct transaction_data
   {
       transaction_data():transmit_count(0){}
       transaction_data( const signed_transaction& t ):trx(t),transmit_count(0){}

       signed_transaction       trx;
       std::string              memo_string;
       fc::time_point           received_time;
       /** the number of times this transaction has been transmitted */
       uint32_t                 transmit_count;
   };

 

   /** cached blockchain data */
   typedef wallet_record< bts::blockchain::asset_record,   asset_record_type       >  wallet_asset_record;
   typedef wallet_record< bts::blockchain::name_record,    name_record_type        >  wallet_name_record;
   typedef wallet_record< bts::blockchain::balance_record, balance_record_type     >  wallet_balance_record;

   /** records unique to the wallet */
   typedef wallet_record< transaction_data,                transaction_record_type >  wallet_transaction_record;
   typedef wallet_record< master_key,                      master_key_record_type  >  wallet_master_key_record;
   typedef wallet_record< key_data,                        key_record_type         >  wallet_key_record;
   typedef wallet_record< account,                         account_record_type     >  wallet_account_record;
   typedef wallet_record< wallet_property,                 property_record_type    >  wallet_property_record;

   typedef fc::optional<wallet_transaction_record>     owallet_transaction_record;
   typedef fc::optional<wallet_master_key_record>      owallet_master_key_record;
   typedef fc::optional<wallet_key_record>             owallet_key_record;
   typedef fc::optional<wallet_account_record>         owallet_account_record;
   typedef fc::optional<wallet_property_record>        owallet_property_record;
   typedef fc::optional<wallet_balance_record>         owallet_balance_record;

   namespace detail { class wallet_db_impl; }

   class wallet_db
   {
      public:
         wallet_db();
         ~wallet_db();
         
         void open( const fc::path& wallet_file );
         void close();
         
         bool is_open()const;

         template<typename T>
         void store_record( T t )
         {
            if( t.index == 0 ) 
               t.index = new_index();
            store_generic_record( t.index, generic_wallet_record( t ) );
         }
         int32_t              new_index();
         int32_t              new_key_child_index();
         fc::ecc::private_key new_private_key( const fc::sha512& password, 
                                               const address& parent_account_address = address() );

         void        set_property( property_enum property_id, const fc::variant& v );
         fc::variant get_property( property_enum property_id );

         void store_key( const key_data& k );
         void store_transaction( const transaction_data& t );
         void cache_balance( const bts::blockchain::balance_record& );
         void cache_memo( const memo_status& memo, 
                          const fc::ecc::private_key& account_key,
                          const fc::sha512& password );

         private_keys get_account_private_keys( const fc::sha512& password );

         owallet_account_record lookup_account( const address& address_of_public_key );
         owallet_account_record lookup_account( const std::string& account_name );
         fc::optional<fc::ecc::private_key>  lookup_private_key( const address& address, 
                                                                 const fc::sha512& password );
         owallet_balance_record lookup_balance( const balance_id_type& balance_id );
         owallet_key_record     lookup_key( const address& address );

         bool has_private_key( const address& a )const;

         void add_account( const std::string& new_account_name, 
                           const public_key_type& new_account_key );

         void rename_account( const std::string& old_account_name,
                              const std::string& new_account_name );


         fc::optional<wallet_master_key_record>                 wallet_master_key;

         std::unordered_map<address,wallet_key_record>          keys;
         std::unordered_map<address,int32_t>                    address_to_account;
         std::unordered_map<name_id_type,int32_t>               name_id_to_account;
         std::map<std::string,int32_t>                          name_to_account;

         std::unordered_map< int32_t,wallet_account_record >                   accounts;
         std::unordered_map< transaction_id_type, wallet_transaction_record >  transactions;
         std::unordered_map< balance_id_type,wallet_balance_record >           balances;
         std::map<std::string,wallet_name_record>                            names;
         std::map<std::string,wallet_asset_record>                           assets;
         std::map<property_enum, wallet_property_record>                     properties;

         void export_to_json( const fc::path& export_file_name ) const;

         void create_from_json( const fc::path& file_to_import, 
                                const fc::path& wallet_to_create );

      private:
        void store_generic_record( int32_t index, const generic_wallet_record& r );
        std::unique_ptr<detail::wallet_db_impl> my;
   };

} } // bts::wallet

FC_REFLECT_ENUM( bts::wallet::property_enum,
                    (next_record_number)
                    (default_transaction_fee)
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
                   (name_record_type)
                   (asset_record_type)
                   (property_record_type)
                )
FC_REFLECT( bts::wallet::wallet_property, (key)(value) )
FC_REFLECT( bts::wallet::generic_wallet_record, (type)(data) )
FC_REFLECT( bts::wallet::master_key, (encrypted_key)(checksum) )
FC_REFLECT( bts::wallet::key_data, (account_address)(public_key)(encrypted_private_key)(memo) )
FC_REFLECT( bts::wallet::transaction_data, (trx)(memo_string)(received_time)(transmit_count) )
FC_REFLECT( bts::wallet::account, (name)(registered_name_id)(account_address) )


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
            typedef decltype(((bts::wallet::base_record<N>*)nullptr)->index) member_type;  
            visitor.TEMPLATE operator()<member_type,bts::wallet::base_record<N>,&bts::wallet::base_record<N>::index>( "index" ); 
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
