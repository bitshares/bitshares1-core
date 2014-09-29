#pragma once

#include <bts/blockchain/types.hpp>
#include <bts/blockchain/asset.hpp>

#include <fc/time.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {

   enum account_type
   {
      titan_account  = 0,
      public_account = 1
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
      delegate_stats( share_type pr = -1 ):pay_rate(pr){}

      share_type        votes_for = 0;
      uint32_t          blocks_produced = 0;
      uint32_t          blocks_missed = 0;
      secret_hash_type  next_secret_hash;
      uint32_t          last_block_num_produced = 0;
      /**
       *  XTS per block produced
       */
      share_type        pay_rate = -1;

      /**
       *  Delegate pay is held in escrow and may be siezed 
       *  and returned to the shareholders if they are fired
       *  for provable cause.
       */
      share_type        pay_balance = 0;
   };

   typedef fc::optional<delegate_stats> odelegate_stats;

   struct account_record
   {
      bool              is_null()const;
      account_record    make_null()const;

      share_type        delegate_pay_balance()const;
      bool              is_delegate()const;
      void              adjust_votes_for( share_type delta );
      share_type        net_votes()const;
      bool              is_retracted()const;
      address           active_address()const;
      void              set_active_key( const time_point_sec& now, const public_key_type& new_key );
      public_key_type   active_key()const;
      share_type        delegate_pay_rate()const;

      account_id_type                        id = 0;
      std::string                            name;
      fc::variant                            public_data;
      public_key_type                        owner_key;
      map<time_point_sec, public_key_type>   active_key_history;
      fc::time_point_sec                     registration_date;
      fc::time_point_sec                     last_update;
      optional<delegate_stats>               delegate_info;
      optional<account_meta_info>            meta_data;
   };
   typedef fc::optional<account_record> oaccount_record;

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
   };

   struct burn_record : public burn_record_key, public burn_record_value
   {
      burn_record(){}
      burn_record( const burn_record_key& k, const burn_record_value& v = burn_record_value())
      :burn_record_key(k),burn_record_value(v){}
   };
   typedef optional<burn_record> oburn_record;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::account_meta_info, (type)(data) )

FC_REFLECT( bts::blockchain::account_record,
            (id)(name)(public_data)(owner_key)(active_key_history)(registration_date)(last_update)(delegate_info)(meta_data) )
FC_REFLECT( bts::blockchain::delegate_stats, 
            (votes_for)(blocks_produced)(blocks_missed)(pay_rate)(pay_balance)(next_secret_hash)(last_block_num_produced) )
FC_REFLECT( bts::blockchain::burn_record_key,   (account_id)(transaction_id) )
FC_REFLECT( bts::blockchain::burn_record_value, (amount)(message)(signer) )
FC_REFLECT_DERIVED( bts::blockchain::burn_record, (bts::blockchain::burn_record_key)(bts::blockchain::burn_record_value), BOOST_PP_SEQ_NIL )
FC_REFLECT_ENUM( bts::blockchain::account_type, (titan_account)(public_account) )

