#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

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

   struct name_record
   {
      name_record()
      :id(0){}

      static bool is_valid_name( const std::string& str );
      static bool is_valid_json( const std::string& str );

      bool is_null()const { return owner_key == public_key_type(); }
      name_record make_null()const    { name_record cpy(*this); cpy.owner_key = public_key_type(); return cpy;      }

      share_type delegate_pay_balance()const
      { // TODO: move to cpp
         FC_ASSERT( is_delegate() );
         return delegate_info->pay_balance;
      }
      bool    is_delegate()const { return !!delegate_info; }
      int64_t net_votes()const 
      {  // TODO: move to cpp
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_for - delegate_info->votes_against; 
      }
      void adjust_votes_for( share_type delta )
      {
         FC_ASSERT( is_delegate() );
         delegate_info->votes_for += delta;
      }
      void adjust_votes_against( share_type delta )
      {
         FC_ASSERT( is_delegate() );
         delegate_info->votes_against += delta;
      }
      share_type votes_for()const 
      {
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_for;
      }
      share_type votes_against()const 
      {
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_against;
      }
      bool is_retracted()const { return active_key == public_key_type(); }

      name_id_type                 id;
      std::string                  name;
      fc::variant                  json_data;
      public_key_type              owner_key;
      public_key_type              active_key;
      fc::time_point_sec           registration_date;
      fc::time_point_sec           last_update;
      fc::optional<delegate_stats> delegate_info;
   };
   typedef fc::optional<name_record> oname_record;
} }

FC_REFLECT( bts::blockchain::name_record,
            (id)(name)(json_data)(owner_key)(active_key)(delegate_info)(registration_date)(last_update)
          )
FC_REFLECT( bts::blockchain::delegate_stats, 
            (votes_for)(votes_against)(blocks_produced)
            (blocks_missed)(pay_balance)(next_secret_hash)(last_block_num_produced) )
