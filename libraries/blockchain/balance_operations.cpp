#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

   asset balance_record::calculate_yield( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const
   {
      if( amount <= 0 )       return asset(0,condition.asset_id);
      if( share_supply <= 0 ) return asset(0,condition.asset_id);
      if( yield_pool <= 0 )   return asset(0,condition.asset_id);

      auto elapsed_time = (now - deposit_date);
      if(  elapsed_time > fc::seconds( BTS_BLOCKCHAIN_MIN_YIELD_PERIOD_SEC ) )
      {
         if( yield_pool > 0 && share_supply > 0 )
         {
            fc::uint128 amount_withdrawn( amount );
            amount_withdrawn *= 1000000;

            fc::uint128 current_supply( share_supply - yield_pool );
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

   balance_id_type deposit_operation::balance_id()const
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
      if( this->slate.supported_delegates.size() > BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
         FC_CAPTURE_AND_THROW( too_may_delegates_in_slate, (slate.supported_delegates.size()) );

      const slate_id_type slate_id = this->slate.id();
      const odelegate_slate current_slate = eval_state._current_state->get_delegate_slate( slate_id );
      if( NOT current_slate.valid() )
      {
         for( const signed_int& delegate_id : this->slate.supported_delegates )
            eval_state.verify_delegate_id( abs( delegate_id ) );

         eval_state._current_state->store_delegate_slate( slate_id, slate );
      }
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void deposit_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

       const balance_id_type deposit_balance_id = this->balance_id();

       obalance_record cur_record = eval_state._current_state->get_balance_record( deposit_balance_id );
       if( !cur_record.valid() )
       {
          cur_record = balance_record( this->condition );
       }

       if( cur_record->balance == 0 )
       {
          cur_record->deposit_date = eval_state._current_state->now();
       }
       else
       {
          fc::uint128 old_sec_since_epoch( cur_record->deposit_date.sec_since_epoch() );
          fc::uint128 new_sec_since_epoch( eval_state._current_state->now().sec_since_epoch() );

          fc::uint128 avg = (old_sec_since_epoch * cur_record->balance) + (new_sec_since_epoch * this->amount);
          avg /= (cur_record->balance + this->amount);

          cur_record->deposit_date = time_point_sec( avg.to_integer() );
       }

       cur_record->balance += this->amount;
       eval_state.sub_balance( deposit_balance_id, asset( this->amount, cur_record->condition.asset_id ) );

       if( cur_record->condition.asset_id == 0 && cur_record->condition.delegate_slate_id )
          eval_state.adjust_vote( cur_record->condition.delegate_slate_id, this->amount );

       cur_record->last_update = eval_state._current_state->now();

       eval_state._current_state->store_balance_record( *cur_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void withdraw_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_withdraw, (amount) );

      obalance_record current_balance_record = eval_state._current_state->get_balance_record( this->balance_id );
      if( !current_balance_record.valid() )
         FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

      if( this->amount > current_balance_record->get_spendable_balance( eval_state._current_state->now() ).amount )
         FC_CAPTURE_AND_THROW( insufficient_funds, (current_balance_record)(amount) );

      switch( (withdraw_condition_types)current_balance_record->condition.type )
      {
         case withdraw_signature_type:
         {
             const withdraw_with_signature condition = current_balance_record->condition.as<withdraw_with_signature>();
             const address owner = condition.owner;
             if( !eval_state.check_signature( owner ) )
                 FC_CAPTURE_AND_THROW( missing_signature, (owner) );
             break;
         }

         case withdraw_vesting_type:
         {
             const withdraw_vesting condition = current_balance_record->condition.as<withdraw_vesting>();
             const address owner = condition.owner;
             if( !eval_state.check_signature( owner ) )
                 FC_CAPTURE_AND_THROW( missing_signature, (owner) );
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
            FC_ASSERT( !"Not supported yet!" );

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
            FC_ASSERT( !"Not supported yet!" );

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

         default:
            FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (current_balance_record->condition) );
      }

      // update delegate vote on withdrawn account..
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

      current_balance_record->balance -= this->amount;
      eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );

      current_balance_record->last_update = eval_state._current_state->now();

      eval_state._current_state->store_balance_record( *current_balance_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void withdraw_all_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       FC_ASSERT( !"Not implemented!" );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void burn_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( message.size() )
          FC_ASSERT( amount.asset_id == 0 );

      if( amount.asset_id == 0 )
      {
          FC_ASSERT( amount.amount >= BTS_BLOCKCHAIN_MIN_BURN_FEE, "",
                     ("amount",amount)
                     ("BTS_BLOCKCHAIN_MIN_BURN_FEE",BTS_BLOCKCHAIN_MIN_BURN_FEE) );
      }

      oasset_record asset_rec = eval_state._current_state->get_asset_record( amount.asset_id );
      FC_ASSERT( asset_rec.valid() );
      FC_ASSERT( !asset_rec->is_market_issued() );

      asset_rec->current_share_supply -= this->amount.amount;
      eval_state.sub_balance( address(), this->amount );

      eval_state._current_state->store_asset_record( *asset_rec );

      if( account_id != 0 ) // you can offer burnt offerings to God if you like... otherwise it must be an account
      {
          const oaccount_record account_rec = eval_state._current_state->get_account_record( abs( this->account_id ) );
          FC_ASSERT( account_rec.valid() );
      }
      eval_state._current_state->store_burn_record( burn_record( burn_record_key( {account_id, eval_state.trx.id()} ),
                                                                 burn_record_value( {amount,message,message_signature} ) ) );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void release_escrow_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      FC_ASSERT( !"Release escrow operation is not enabled yet!" );

      auto escrow_balance_record = eval_state._current_state->get_balance_record( this->escrow_id );
      FC_ASSERT( escrow_balance_record.valid() );

      if( !eval_state.check_signature( this->released_by ) )
         FC_ASSERT( !"transaction not signed by releasor" );

      auto escrow_condition = escrow_balance_record->condition.as<withdraw_with_escrow>();
      auto total_released = amount_to_sender + amount_to_receiver;

      FC_ASSERT( total_released <= escrow_balance_record->balance );
      FC_ASSERT( total_released >= amount_to_sender ); // check for addition overflow

      escrow_balance_record->balance -= total_released;

      if( escrow_condition.sender == this->released_by )
      {
         FC_ASSERT( amount_to_sender == 0 );
         FC_ASSERT( amount_to_receiver <= escrow_balance_record->balance );

         if( !eval_state.check_signature( escrow_condition.sender ) )
             FC_CAPTURE_AND_THROW( missing_signature, (escrow_condition.sender) );

         balance_record new_balance_record( escrow_condition.receiver, 
                                            asset( amount_to_receiver, escrow_balance_record->asset_id() ),
                                            escrow_balance_record->delegate_slate_id() );
         auto current_receiver_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

         if( current_receiver_balance )
            current_receiver_balance->balance += amount_to_receiver;
         else
            current_receiver_balance = new_balance_record;

          eval_state._current_state->store_balance_record( *current_receiver_balance );
      }
      else if( escrow_condition.receiver == this->released_by )
      {
         FC_ASSERT( amount_to_receiver == 0 );
         FC_ASSERT( amount_to_sender <= escrow_balance_record->balance );

         if( !eval_state.check_signature( escrow_condition.sender ) )
             FC_CAPTURE_AND_THROW( missing_signature, (escrow_condition.receiver) );

         balance_record new_balance_record( escrow_condition.sender, 
                                            asset( amount_to_sender, escrow_balance_record->asset_id() ),
                                            escrow_balance_record->delegate_slate_id() );
         auto current_sender_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

         if( current_sender_balance )
            current_sender_balance->balance += amount_to_sender;
         else
            current_sender_balance = new_balance_record;

         eval_state._current_state->store_balance_record( *current_sender_balance );
      }
      else if( escrow_condition.escrow == this->released_by )
      {
         if( !eval_state.check_signature( escrow_condition.escrow ) )
             FC_CAPTURE_AND_THROW( missing_signature, (escrow_condition.escrow) );
         // get a balance record for the receiver, create it if necessary and deposit funds
         {
            balance_record new_balance_record( escrow_condition.receiver, 
                                               asset( amount_to_receiver, escrow_balance_record->asset_id() ),
                                               escrow_balance_record->delegate_slate_id() );
            auto current_receiver_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

            if( current_receiver_balance )
               current_receiver_balance->balance += amount_to_receiver;
            else
               current_receiver_balance = new_balance_record;
            eval_state._current_state->store_balance_record( *current_receiver_balance );
         }
         //  get a balance record for the sender, create it if necessary and deposit funds
         {
            balance_record new_balance_record( escrow_condition.sender, 
                                               asset( amount_to_sender, escrow_balance_record->asset_id() ),
                                               escrow_balance_record->delegate_slate_id() );
            auto current_sender_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

            if( current_sender_balance )
               current_sender_balance->balance += amount_to_sender;
            else
               current_sender_balance = new_balance_record;
            eval_state._current_state->store_balance_record( *current_sender_balance );
         }
      }
      else if( address() == this->released_by )
      {
         if( !eval_state.check_signature( escrow_condition.sender ) )
             FC_CAPTURE_AND_THROW( missing_signature, (escrow_condition.sender) );
         if( !eval_state.check_signature( escrow_condition.receiver ) )
             FC_CAPTURE_AND_THROW( missing_signature, (escrow_condition.receiver) );
         // get a balance record for the receiver, create it if necessary and deposit funds
         {
            balance_record new_balance_record( escrow_condition.receiver, 
                                               asset( amount_to_receiver, escrow_balance_record->asset_id() ),
                                               escrow_balance_record->delegate_slate_id() );
            auto current_receiver_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

            if( current_receiver_balance )
               current_receiver_balance->balance += amount_to_receiver;
            else
               current_receiver_balance = new_balance_record;
            eval_state._current_state->store_balance_record( *current_receiver_balance );
         }
         //  get a balance record for the sender, create it if necessary and deposit funds
         {
            balance_record new_balance_record( escrow_condition.sender, 
                                               asset( amount_to_sender, escrow_balance_record->asset_id() ),
                                               escrow_balance_record->delegate_slate_id() );
            auto current_sender_balance = eval_state._current_state->get_balance_record( new_balance_record.id());

            if( current_sender_balance )
               current_sender_balance->balance += amount_to_sender;
            else
               current_sender_balance = new_balance_record;
            eval_state._current_state->store_balance_record( *current_sender_balance );
         }
      }
      else
      {
          FC_ASSERT( !"not released by a party to the escrow transaction" );
      }

      eval_state._current_state->store_balance_record( *escrow_balance_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
