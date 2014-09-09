#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   asset balance_record::calculate_yield( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const
   {
      if( amount <= 0 )       return asset(0,condition.asset_id);
      if( share_supply <= 0 ) return asset(0,condition.asset_id);
      if( yield_pool <= 0 ) return asset(0,condition.asset_id);

      auto elapsed_time = (now - deposit_date);
      if(  elapsed_time > fc::seconds( BTS_BLOCKCHAIN_MIN_YIELD_PERIOD_SEC ) )
      {
         if( yield_pool > 0 && share_supply > 0 )
         {
            fc::uint128 amount_withdrawn( amount );
            amount_withdrawn *= 1000000;

            fc::uint128 current_supply( share_supply );
            fc::uint128 fee_fund( yield_pool );

            auto yield = (amount_withdrawn * fee_fund) / current_supply;

            /**
             *  If less than 1 year, the 80% of yield are scaled linearly with time and 20% scaled by time^2,
             *  this should produce a yield curve that is "80% simple interest" and 20% simulating compound
             *  interest.
             */
            if( elapsed_time < fc::seconds( BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC ) )
            {
               auto original_yield = yield;
               // discount the yield by 80%
               yield *= 8;
               yield /= 10;

               auto delta_yield = original_yield - yield;

               // yield == amount withdrawn / total usd  *  fee fund * fraction_of_year
               yield *= elapsed_time.to_seconds();
               yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

               delta_yield *= elapsed_time.to_seconds();
               delta_yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

               delta_yield *= elapsed_time.to_seconds();
               delta_yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

               yield += delta_yield;
            }

            yield /= 1000000;
            auto yield_amount = yield.to_uint64();

            if( yield_amount > 0 && yield_amount < yield_pool )
            {
               return asset( yield_amount, condition.asset_id );
            }
         }
      }
      return asset( 0, condition.asset_id );
   }

   balance_id_type  deposit_operation::balance_id()const
   {
      return condition.get_address();
   }

   deposit_operation::deposit_operation( const address& owner,
                                         const asset& amnt,
                                         slate_id_type slate_id )
   {
      FC_ASSERT( amnt.amount > 0 );
      amount = amnt.amount;
      condition = withdraw_condition( withdraw_with_signature( owner ),
                                      amnt.asset_id, slate_id );
   }

   void define_delegate_slate_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      auto slate_id = this->slate.id();

      if( this->slate.supported_delegates.size() > BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
         FC_CAPTURE_AND_THROW( too_may_delegates_in_slate, (slate.supported_delegates.size()) );

      auto current_slate = eval_state._current_state->get_delegate_slate( slate_id );
      if( NOT current_slate )
      {
         for( auto delegate_id : this->slate.supported_delegates )
         {
            eval_state.verify_delegate_id( abs(delegate_id) );
         }
         eval_state._current_state->store_delegate_slate( slate_id, slate );
      }
   } FC_CAPTURE_AND_RETHROW( (eval_state) ) }

   /**
    *  TODO: Document Rules for Deposits
    *
    */
   void deposit_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       if( eval_state._current_state->get_head_block_num() < BTSX_MARKET_FORK_6_BLOCK_NUM )
       {
          evaluate_v1( eval_state );
          return;
       }

       if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

       auto deposit_balance_id = this->balance_id();

       auto cur_record = eval_state._current_state->get_balance_record( deposit_balance_id );
       if( !cur_record )
       {
          cur_record = balance_record( this->condition );
       }
       cur_record->last_update   = eval_state._current_state->now();
       if( cur_record->balance == 0 )
       {
          cur_record->deposit_date  = eval_state._current_state->now();
       }
       else
       {
          fc::uint128 old_sec_since_epoch( cur_record->deposit_date.sec_since_epoch());
          fc::uint128 new_sec_since_epoch( eval_state._current_state->now().sec_since_epoch());

          fc::uint128 avg = old_sec_since_epoch * cur_record->balance + new_sec_since_epoch*this->amount;
          avg /= (cur_record->balance + this->amount);

          cur_record->deposit_date  =  time_point_sec( avg.to_integer() );
       }
       cur_record->balance       += this->amount;

       eval_state.sub_balance( deposit_balance_id, asset(this->amount, cur_record->condition.asset_id) );

       if( cur_record->condition.asset_id == 0 && cur_record->condition.delegate_slate_id )
          eval_state.adjust_vote( cur_record->condition.delegate_slate_id, this->amount );

       eval_state._current_state->store_balance_record( *cur_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void deposit_operation::evaluate_v1( transaction_evaluation_state& eval_state )
   { try {
       if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

       auto deposit_balance_id = this->balance_id();

       auto cur_record = eval_state._current_state->get_balance_record( deposit_balance_id );
       if( !cur_record )
       {
          cur_record = balance_record( this->condition );
       }
       cur_record->last_update   = eval_state._current_state->now();
       cur_record->balance       += this->amount;

       eval_state.sub_balance( deposit_balance_id, asset(this->amount, cur_record->condition.asset_id) );

       if( cur_record->condition.asset_id == 0 && cur_record->condition.delegate_slate_id )
          eval_state.adjust_vote( cur_record->condition.delegate_slate_id, this->amount );

       eval_state._current_state->store_balance_record( *cur_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  TODO: Document rules for Withdraws
    */
   void withdraw_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( eval_state._current_state->get_head_block_num() < BTSX_MARKET_FORK_6_BLOCK_NUM )
      {
         evaluate_v1( eval_state );
         return;
      }

      if( this->amount <= 0 )
         FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

      obalance_record current_balance_record = eval_state._current_state->get_balance_record( this->balance_id );

      if( !current_balance_record )
         FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

      if( this->amount > current_balance_record->balance )
         FC_CAPTURE_AND_THROW( insufficient_funds,
                               (current_balance_record)
                               (amount)
                               (current_balance_record->balance - amount) );

      switch( (withdraw_condition_types)current_balance_record->condition.type )
      {
         case withdraw_signature_type:
         {
            auto owner = current_balance_record->condition.as<withdraw_with_signature>().owner;
            if( claim_input_data.size() )
            {
               auto pts_sig = fc::raw::unpack<withdraw_with_pts>( claim_input_data );
               fc::sha256::encoder enc;
               fc::raw::pack( enc, eval_state._current_state->get_property( chain_property_enum::chain_id ).as<fc::ripemd160>() );
               fc::raw::pack( enc, pts_sig.new_key );
               auto digest = enc.result();
               fc::ecc::public_key test_key( pts_sig.pts_signature, digest );
               if( address(test_key) == owner ||
                   address(pts_address(test_key,false,56) ) == owner ||
                   address(pts_address(test_key,true,56) ) == owner ||
                   address(pts_address(test_key,false,0) ) == owner ||
                   address(pts_address(test_key,true,0) ) == owner )
               {
                  if( !eval_state.check_signature( pts_sig.new_key) )
                     FC_CAPTURE_AND_THROW( missing_signature, (pts_sig) );
                  break;
               }
               else
               {
                  FC_CAPTURE_AND_THROW( missing_signature, (owner)(pts_sig) );
               }
            }
            else
            {
               if( !eval_state.check_signature( owner ) )
                  FC_CAPTURE_AND_THROW( missing_signature, (owner) );
            }
            break;
         }

         case withdraw_multi_sig_type:
         {
            auto multi_sig = current_balance_record->condition.as<withdraw_with_multi_sig>();
            uint32_t valid_signatures = 0;
            for( auto sig : multi_sig.owners )
               valid_signatures += eval_state.check_signature( sig );
            if( valid_signatures < multi_sig.required )
               FC_CAPTURE_AND_THROW( missing_signature, (valid_signatures)(multi_sig) );
            break;
         }

         case withdraw_password_type:
         {
            auto password_condition = current_balance_record->condition.as<withdraw_with_password>();
            try {
               if( password_condition.timeout < eval_state._current_state->now() )
               {
                  if( !eval_state.check_signature( password_condition.payor ) )
                     FC_CAPTURE_AND_THROW( missing_signature, (password_condition.payor) );
               }
               else
               {
                  if( !eval_state.check_signature( password_condition.payee ) )
                     FC_CAPTURE_AND_THROW( missing_signature, (password_condition.payee) );
                  if( claim_input_data.size() < sizeof( fc::ripemd160 ) )
                     FC_CAPTURE_AND_THROW( invalid_claim_password, (claim_input_data) );

                  auto input_password_hash = fc::ripemd160::hash( claim_input_data.data(),
                                                                  claim_input_data.size() );

                  if( password_condition.password_hash != input_password_hash )
                     FC_CAPTURE_AND_THROW( invalid_claim_password, (input_password_hash) );
               }
            } FC_CAPTURE_AND_RETHROW( (password_condition ) )
            break;
         }

         case withdraw_option_type:
         {
            auto option = current_balance_record->condition.as<withdraw_option>();
            try {
               if( eval_state._current_state->now() > option.date )
               {
                  if( !eval_state.check_signature( option.optionor ) )
                     FC_CAPTURE_AND_THROW( missing_signature, (option.optionor) );
               }
               else // the option hasn't expired
               {
                  if( !eval_state.check_signature( option.optionee ) )
                     FC_CAPTURE_AND_THROW( missing_signature, (option.optionee) );

                  auto pay_amount = asset( this->amount, current_balance_record->condition.asset_id ) * option.strike_price;
                  eval_state.add_required_deposit( option.optionee, pay_amount );
               }
            } FC_CAPTURE_AND_RETHROW( (option) )
            break;
         }
         case withdraw_null_type:
            FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (current_balance_record->condition) );
            break;
      //   default:
      }
      // update delegate vote on withdrawn account..

      current_balance_record->balance -= this->amount;
      current_balance_record->last_update = eval_state._current_state->now();

      if( current_balance_record->condition.asset_id == 0 && current_balance_record->condition.delegate_slate_id )
         eval_state.adjust_vote( current_balance_record->condition.delegate_slate_id, -this->amount );

      auto asset_rec = eval_state._current_state->get_asset_record( current_balance_record->condition.asset_id );
      FC_ASSERT( asset_rec.valid() );
      if( asset_rec->is_market_issued() )
      {
         auto yield = current_balance_record->calculate_yield( eval_state._current_state->now(),
                                                               current_balance_record->balance,
                                                               asset_rec->collected_fees,
                                                               asset_rec->current_share_supply );
         if( yield.amount > 0 )
         {
            asset_rec->collected_fees       -= yield.amount;
            current_balance_record->balance += yield.amount;
            current_balance_record->deposit_date = eval_state._current_state->now();
            eval_state.yield[current_balance_record->condition.asset_id] += yield.amount;
            eval_state._current_state->store_asset_record( *asset_rec );
         }
      }

      eval_state._current_state->store_balance_record( *current_balance_record );
      eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void withdraw_operation::evaluate_v1( transaction_evaluation_state& eval_state )
   { try {
       if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

      obalance_record current_balance_record = eval_state._current_state->get_balance_record( this->balance_id );

      if( eval_state._current_state->get_head_block_num() >= BTSX_MARKET_FORK_4_BLOCK_NUM )
      {
         if( !current_balance_record )
            FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

         if( this->amount > current_balance_record->balance )
            FC_CAPTURE_AND_THROW( insufficient_funds,
                                  (current_balance_record)
                                  (amount)
                                  (current_balance_record->balance - amount) );
      }

      if( current_balance_record )
      {
         switch( (withdraw_condition_types)current_balance_record->condition.type )
         {
            case withdraw_signature_type:
            {
               auto owner = current_balance_record->condition.as<withdraw_with_signature>().owner;
               if( claim_input_data.size() )
               {
                  auto pts_sig = fc::raw::unpack<withdraw_with_pts>( claim_input_data );
                  fc::sha256::encoder enc;
                  fc::raw::pack( enc, eval_state._current_state->get_property( chain_property_enum::chain_id ).as<fc::ripemd160>() );
                  fc::raw::pack( enc, pts_sig.new_key );
                  auto digest = enc.result();
                  fc::ecc::public_key test_key( pts_sig.pts_signature, digest );
                  if( address(test_key) == owner ||
                      address(pts_address(test_key,false,56) ) == owner ||
                      address(pts_address(test_key,true,56) ) == owner ||
                      address(pts_address(test_key,false,0) ) == owner ||
                      address(pts_address(test_key,true,0) ) == owner )
                  {
                     if( !eval_state.check_signature( pts_sig.new_key) )
                        FC_CAPTURE_AND_THROW( missing_signature, (pts_sig) );
                     break;
                  }
                  else
                  {
                     FC_CAPTURE_AND_THROW( missing_signature, (owner)(pts_sig) );
                  }
               }
               else
               {
                  if( !eval_state.check_signature( owner ) )
                     FC_CAPTURE_AND_THROW( missing_signature, (owner) );
               }
               break;
            }

            case withdraw_multi_sig_type:
            {
               auto multi_sig = current_balance_record->condition.as<withdraw_with_multi_sig>();
               uint32_t valid_signatures = 0;
               for( auto sig : multi_sig.owners )
                  valid_signatures += eval_state.check_signature( sig );
               if( valid_signatures < multi_sig.required )
                  FC_CAPTURE_AND_THROW( missing_signature, (valid_signatures)(multi_sig) );
               break;
            }

            case withdraw_password_type:
            {
               auto password_condition = current_balance_record->condition.as<withdraw_with_password>();
               try {
                  if( password_condition.timeout < eval_state._current_state->now() )
                  {
                     if( !eval_state.check_signature( password_condition.payor ) )
                        FC_CAPTURE_AND_THROW( missing_signature, (password_condition.payor) );
                  }
                  else
                  {
                     if( !eval_state.check_signature( password_condition.payee ) )
                        FC_CAPTURE_AND_THROW( missing_signature, (password_condition.payee) );
                     if( claim_input_data.size() < sizeof( fc::ripemd160 ) )
                        FC_CAPTURE_AND_THROW( invalid_claim_password, (claim_input_data) );

                     auto input_password_hash = fc::ripemd160::hash( claim_input_data.data(),
                                                                     claim_input_data.size() );

                     if( password_condition.password_hash != input_password_hash )
                        FC_CAPTURE_AND_THROW( invalid_claim_password, (input_password_hash) );
                  }
               } FC_CAPTURE_AND_RETHROW( (password_condition ) )
               break;
            }

            case withdraw_option_type:
            {
               auto option = current_balance_record->condition.as<withdraw_option>();
               try {
                  if( eval_state._current_state->now() > option.date )
                  {
                     if( !eval_state.check_signature( option.optionor ) )
                        FC_CAPTURE_AND_THROW( missing_signature, (option.optionor) );
                  }
                  else // the option hasn't expired
                  {
                     if( !eval_state.check_signature( option.optionee ) )
                        FC_CAPTURE_AND_THROW( missing_signature, (option.optionee) );

                     auto pay_amount = asset( this->amount, current_balance_record->condition.asset_id ) * option.strike_price;
                     eval_state.add_required_deposit( option.optionee, pay_amount );
                  }
               } FC_CAPTURE_AND_RETHROW( (option) )
               break;
            }
            case withdraw_null_type:
               FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (current_balance_record->condition) );
               break;
         //   default:
         }
         // update delegate vote on withdrawn account..

         current_balance_record->balance -= this->amount;
         current_balance_record->last_update = eval_state._current_state->now();

         if( current_balance_record->condition.asset_id == 0 && current_balance_record->condition.delegate_slate_id )
            eval_state.adjust_vote( current_balance_record->condition.delegate_slate_id, -this->amount );

         eval_state._current_state->store_balance_record( *current_balance_record );
         eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );
      } // if current balance record
      else
      {
         eval_state.add_balance( asset(this->amount, 0) );
      }
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
