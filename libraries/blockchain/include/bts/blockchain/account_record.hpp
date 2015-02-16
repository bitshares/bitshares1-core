#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/delegate_record.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

enum account_type
{
   titan_account    = 0,
   public_account   = 1,
   multisig_account = 2
};

struct multisig_meta_info
{
   static const account_type type = multisig_account;

   uint32_t                required = 0;
   std::set<address>       owners;
};

struct account_meta_info
{
   fc::enum_type<fc::unsigned_int,account_type> type = public_account;
   vector<char>                                 data;

   account_meta_info( account_type atype = public_account )
   :type( atype ){}

   template<typename AccountType>
   account_meta_info(  const AccountType& t )
   :type( AccountType::type )
   {
      data = fc::raw::pack( t );
   }

   template<typename AccountType>
   AccountType as()const
   {
      FC_ASSERT( type == AccountType::type, "", ("AccountType",AccountType::type) );
      return fc::raw::unpack<AccountType>(data);
   }
};

struct account_record;
typedef fc::optional<account_record> oaccount_record;

class chain_interface;
struct account_record
{
   address           owner_address()const { return address( owner_key ); }

   void              set_active_key( const time_point_sec now, const public_key_type& new_key );
   public_key_type   active_key()const;
   address           active_address()const;
   bool              is_retracted()const;

   bool              is_delegate()const;

   void              set_signing_key( uint32_t block_num, const public_key_type& signing_key );
   public_key_type   signing_key()const;
   address           signing_address()const;

   bool              is_titan_account()const { return meta_data.valid() && meta_data->type == titan_account; }

   account_id_type                      id = 0;
   std::string                          name;
   fc::variant                          public_data;
   public_key_type                      owner_key;
   map<time_point_sec, public_key_type> active_key_history;
   fc::time_point_sec                   registration_date;
   fc::time_point_sec                   last_update;
   map<uint32_t, public_key_type>       signing_key_history;
   optional<account_meta_info>          meta_data;

   void sanity_check( const chain_interface& )const;
   static oaccount_record lookup( const chain_interface&, const account_id_type );
   static oaccount_record lookup( const chain_interface&, const string& );
   static oaccount_record lookup( const chain_interface&, const address& );
   static void store( chain_interface&, const account_id_type, const account_record& );
   static void remove( chain_interface&, const account_id_type );
};

class account_db_interface
{
   friend struct account_record;

   virtual oaccount_record account_lookup_by_id( const account_id_type )const = 0;
   virtual oaccount_record account_lookup_by_name( const string& )const = 0;
   virtual oaccount_record account_lookup_by_address( const address& )const = 0;

   virtual void account_insert_into_id_map( const account_id_type, const account_record& ) = 0;
   virtual void account_insert_into_name_map( const string&, const account_id_type ) = 0;
   virtual void account_insert_into_address_map( const address&, const account_id_type ) = 0;

   virtual void account_erase_from_id_map( const account_id_type ) = 0;
   virtual void account_erase_from_name_map( const string& ) = 0;
   virtual void account_erase_from_address_map( const address& ) = 0;
};

struct extended_account_record : public account_record
{
    extended_account_record() {}

    extended_account_record( const account_record& a, const odelegate_record& d = odelegate_record() )
        : account_record( a ), delegate_info( d ) {}

    odelegate_record delegate_info;
};

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::account_type,
                 (titan_account)
                 (public_account)
                 (multisig_account)
                 )
FC_REFLECT( bts::blockchain::multisig_meta_info,
            (required)
            (owners)
            )
FC_REFLECT( bts::blockchain::account_meta_info,
            (type)
            (data)
            )
FC_REFLECT( bts::blockchain::account_record,
            (id)
            (name)
            (public_data)
            (owner_key)
            (active_key_history)
            (registration_date)
            (last_update)
            (signing_key_history)
            (meta_data)
            )
FC_REFLECT_DERIVED( bts::blockchain::extended_account_record,
                    (bts::blockchain::account_record),
                    (delegate_info)
                    );
