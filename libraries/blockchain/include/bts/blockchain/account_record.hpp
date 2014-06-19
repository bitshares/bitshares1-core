#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   struct account_meta_info
   {
      fc::unsigned_int type;
      vector<char>     data;
   };

   struct delegate_stats
   {
      delegate_stats()
      :votes_for(0),
       votes_against(0),
       blocks_produced(0),
       blocks_missed(0),
       last_block_num_produced(-1),
       pay_balance(0){}

      share_type                     votes_for;
      share_type                     votes_against;
      uint32_t                       blocks_produced;
      uint32_t                       blocks_missed;
      secret_hash_type               next_secret_hash;
      uint32_t                       last_block_num_produced;

      /**
       *  Delegate pay is held in escrow and may be siezed 
       *  and returned to the shareholders if they are fired
       *  for provable cause.
       */
      share_type                     pay_balance;
   };

   struct delegate_block_stats
   {
      delegate_block_stats()
      :missed(true)
      {}

      bool missed;

      /** If we received a signed block within BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC,
       *  then we store the latency */
      fc::optional<uint32_t> latency;
   };

   struct account_record
   {
      account_record()
      :id(0){}

      bool is_null()const;
      account_record make_null()const;

      share_type     delegate_pay_balance()const;
      bool           is_delegate()const;
      int64_t        net_votes()const;
      void           adjust_votes_for( share_type delta );
      void           adjust_votes_against( share_type delta );
      share_type     votes_for()const;
      share_type     votes_against()const;
      bool           is_retracted()const;
      address        active_address()const;
      void           set_active_key( time_point_sec now, 
                                     const public_key_type& new_key );
      public_key_type active_key()const;

      account_id_type                        id;
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
} }
FC_REFLECT( bts::blockchain::account_meta_info, (type)(data) )

FC_REFLECT( bts::blockchain::account_record,
            (id)(name)(public_data)(owner_key)(active_key_history)(registration_date)(last_update)(delegate_info)(meta_data)
          )
FC_REFLECT( bts::blockchain::delegate_stats, 
            (votes_for)(votes_against)(blocks_produced)
            (blocks_missed)(pay_balance)(next_secret_hash)(last_block_num_produced) )
FC_REFLECT( bts::blockchain::delegate_block_stats, (missed)(latency) )
