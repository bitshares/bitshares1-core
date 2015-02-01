#pragma once

#include <bts/blockchain/asset.hpp>
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
      fc::enum_type<fc::unsigned_int,account_type> type;
      vector<char>                                 data;

      account_meta_info( account_type atype = titan_account )
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

   struct delegate_stats
   {
      share_type                        votes_for = 0;

      uint8_t                           pay_rate = 0;
      map<uint32_t, public_key_type>    signing_key_history;

      uint32_t                          last_block_num_produced = 0;
      optional<secret_hash_type>        next_secret_hash;

      share_type                        pay_balance = 0;
      share_type                        total_paid = 0;
      share_type                        total_burned = 0;

      uint32_t                          blocks_produced = 0;
      uint32_t                          blocks_missed = 0;
   };
   typedef fc::optional<delegate_stats> odelegate_stats;

   struct vote_del
   {
      vote_del( int64_t v = 0, account_id_type del = 0 )
      :votes(v),delegate_id(del){}
      int64_t votes = 0;
      account_id_type delegate_id;
      friend bool operator == ( const vote_del& a, const vote_del& b )
      {
         return a.votes == b.votes && a.delegate_id == b.delegate_id;
      }
      friend bool operator < ( const vote_del& a, const vote_del& b )
      {
         if( a.votes != b.votes ) return a.votes > b.votes; /* Reverse so maps sort in descending order */
         return a.delegate_id < b.delegate_id; /* Lowest id wins in ties */
      }
   };

   class chain_interface;
   struct account_db_interface;
   struct account_record
   {
      address           owner_address()const { return address( owner_key ); }

      void              set_active_key( const time_point_sec now, const public_key_type& new_key );
      public_key_type   active_key()const;
      address           active_address()const;
      bool              is_retracted()const;

      bool              is_delegate()const;
      uint8_t           delegate_pay_rate()const;
      void              adjust_votes_for( const share_type delta );
      share_type        net_votes()const;
      share_type        delegate_pay_balance()const;

      void              set_signing_key( uint32_t block_num, const public_key_type& signing_key );
      public_key_type   signing_key()const;
      address           signing_address()const;

      bool              is_public_account()const { return meta_data.valid() && meta_data->type == public_account; }

      account_id_type                        id = 0;
      std::string                            name;
      fc::variant                            public_data;
      public_key_type                        owner_key;
      map<time_point_sec, public_key_type>   active_key_history;
      fc::time_point_sec                     registration_date;
      fc::time_point_sec                     last_update;
      optional<delegate_stats>               delegate_info;
      optional<account_meta_info>            meta_data;

      static const account_db_interface& db_interface( const chain_interface& );
   };
   typedef fc::optional<account_record> oaccount_record;

   struct account_db_interface
   {
       std::function<oaccount_record( const account_id_type )>              lookup_by_id;
       std::function<oaccount_record( const string& )>                      lookup_by_name;
       std::function<oaccount_record( const address& )>                     lookup_by_address;

       std::function<void( const account_id_type, const account_record& )>  insert_into_id_map;
       std::function<void( const string&, const account_id_type )>          insert_into_name_map;
       std::function<void( const address&, const account_id_type )>         insert_into_address_map;
       std::function<void( const vote_del& )>                               insert_into_vote_set;

       std::function<void( const account_id_type )>                         erase_from_id_map;
       std::function<void( const string& )>                                 erase_from_name_map;
       std::function<void( const address& )>                                erase_from_address_map;
       std::function<void( const vote_del& )>                               erase_from_vote_set;

       oaccount_record lookup( const account_id_type )const;
       oaccount_record lookup( const string& )const;
       oaccount_record lookup( const address& )const;
       void store( const account_id_type, const account_record& )const;
       void remove( const account_id_type )const;
   };

   struct burn_record_key
   {
      account_id_type     account_id;
      transaction_id_type transaction_id;
      friend bool operator < ( const burn_record_key& a, const burn_record_key& b )
      {
         return std::tie( a.account_id, a.transaction_id) < std::tie( b.account_id, b.transaction_id);
      }
      friend bool operator == ( const burn_record_key& a, const burn_record_key& b )
      {
         return std::tie( a.account_id, a.transaction_id) == std::tie( b.account_id, b.transaction_id);
      }
   };

   struct burn_record_value
   {
      bool is_null()const { return amount.amount == 0; }
      asset                    amount;
      string                   message;
      optional<signature_type> signer;

      public_key_type signer_key()const;
   };

   struct burn_record : public burn_record_key, public burn_record_value
   {
      burn_record(){}
      burn_record( const burn_record_key& k, const burn_record_value& v = burn_record_value())
      :burn_record_key(k),burn_record_value(v){}
   };
   typedef optional<burn_record> oburn_record;

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
FC_REFLECT( bts::blockchain::delegate_stats,
            (votes_for)
            (pay_rate)
            (signing_key_history)
            (last_block_num_produced)
            (next_secret_hash)
            (pay_balance)
            (total_paid)
            (total_burned)
            (blocks_produced)
            (blocks_missed)
            )
FC_REFLECT( bts::blockchain::account_record,
            (id)
            (name)
            (public_data)
            (owner_key)
            (active_key_history)
            (registration_date)
            (last_update)
            (delegate_info)
            (meta_data)
            )
FC_REFLECT( bts::blockchain::burn_record_key,
            (account_id)
            (transaction_id) )
FC_REFLECT( bts::blockchain::burn_record_value,
            (amount)
            (message)
            (signer) )
FC_REFLECT_DERIVED( bts::blockchain::burn_record,
                    (bts::blockchain::burn_record_key)
                    (bts::blockchain::burn_record_value),
                    BOOST_PP_SEQ_NIL )
