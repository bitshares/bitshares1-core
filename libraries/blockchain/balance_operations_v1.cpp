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
