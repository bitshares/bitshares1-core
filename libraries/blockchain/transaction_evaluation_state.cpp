#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain { 

   transaction_evaluation_state::transaction_evaluation_state( const chain_interface_ptr& current_state, digest_type chain_id )
   :_current_state( current_state ),_chain_id(chain_id),_skip_signature_check(false)
   {
   }

   transaction_evaluation_state::~transaction_evaluation_state()
   {
   }

   void transaction_evaluation_state::reset()
   {
      signed_keys.clear();
      balance.clear();
      deposits.clear();
      withdraws.clear();
      net_delegate_votes.clear();
      required_deposits.clear();
      validation_error.reset();
   }

   bool transaction_evaluation_state::check_signature( const address& a )const
   {
      return  _skip_signature_check || signed_keys.find( a ) != signed_keys.end();
   }

   void transaction_evaluation_state::verify_delegate_id( account_id_type id )const
   {
      auto current_account = _current_state->get_account_record( id );
      if( !current_account ) FC_CAPTURE_AND_THROW( unknown_account_id, (id) );
      if( !current_account->is_delegate() ) FC_CAPTURE_AND_THROW( not_a_delegate, (id) );
   }

   void transaction_evaluation_state::add_required_deposit( const address& owner_key, const asset& amount )
   {
      FC_ASSERT( trx.delegate_slate_id );
      balance_id_type balance_id = withdraw_condition( 
                                       withdraw_with_signature( owner_key ), 
                                       amount.asset_id, *trx.delegate_slate_id ).get_address();

      auto itr = required_deposits.find( balance_id );
      if( itr == required_deposits.end() )
      {
         required_deposits[balance_id] = amount;
      }
      else
      {
         required_deposits[balance_id] += amount;
      }
   }

   void transaction_evaluation_state::update_delegate_votes()
   {
      auto asset_rec = _current_state->get_asset_record( asset_id_type() );

      for( const auto& del_vote : net_delegate_votes )
      {
         auto del_rec = _current_state->get_account_record( del_vote.first );
         FC_ASSERT( !!del_rec );
         del_rec->adjust_votes_for( del_vote.second.votes_for );

         _current_state->store_account_record( *del_rec );
      }
   }

   void transaction_evaluation_state::validate_required_fee()
   { try {
      asset xts_fees;
      auto fee_itr = balance.find( 0 );
      if( fee_itr != balance.end() ) xts_fees += asset( fee_itr->second, 0);
      
      xts_fees += alt_fees_paid;

      if( required_fees > xts_fees )
      {
         FC_CAPTURE_AND_THROW( insufficient_fee, (required_fees)(alt_fees_paid)(xts_fees)  );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    *  Process all fees and update the asset records.
    */
   void transaction_evaluation_state::post_evaluate()
   { try {
      // Should this be here? We may not have fees in XTS now...
      balance[0]; // make sure we have something for this.
      for( const auto& fee : balance )
      {
         if( fee.second < 0 ) FC_CAPTURE_AND_THROW( negative_fee, (fee) );
         // if the fee is already in XTS or the fee balance is zero, move along...
         if( fee.first == 0 || fee.second == 0 )
           continue;

         auto asset_record = _current_state->get_asset_record( fee.first );
         if( !asset_record.valid() ) FC_CAPTURE_AND_THROW( unknown_asset_id, (fee.first) );

         if( !asset_record->is_market_issued() )
           continue;

         // lowest ask is someone with XTS offered at a price of USD / XTS, fee.first
         // is an amount of USD which can be converted to price*USD XTS provided we
         // send lowest_ask.index.owner the USD
         omarket_order lowest_ask = _current_state->get_lowest_ask_record( fee.first, 0 );
         if( lowest_ask )
         {
            // fees paid in something other than XTS are discounted 50% 
            alt_fees_paid += asset( fee.second/2, fee.first ) * lowest_ask->market_index.order_price;  
         }
      }

      for( const auto& fee : balance )
      {
         if( fee.second < 0 ) FC_CAPTURE_AND_THROW( negative_fee, (fee) );
         if( fee.second > 0 ) // if a fee was paid...
         {
            auto asset_record = _current_state->get_asset_record( fee.first );
            if( !asset_record )
              FC_CAPTURE_AND_THROW( unknown_asset_id, (fee.first) );

            asset_record->collected_fees += fee.second;
            // collecting fees should not decrease share supply.
            // asset_record->current_share_supply -= fee.second;
            _current_state->store_asset_record( *asset_record );
         }
      }

      for( const auto& required_deposit : required_deposits )
      {
         auto provided_itr = provided_deposits.find( required_deposit.first );
         
         if( provided_itr->second < required_deposit.second )
            FC_CAPTURE_AND_THROW( missing_deposit, (required_deposit) );
      }

   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg, bool skip_signature_check )
   { try {
      reset();
      _skip_signature_check = skip_signature_check;
      try {
        if( trx_arg.expiration < _current_state->now() )
        {
           auto expired_by_sec = (trx_arg.expiration - _current_state->now()).to_seconds();
           FC_CAPTURE_AND_THROW( expired_transaction, (trx_arg)(_current_state->now())(expired_by_sec) );
        }
        if( trx_arg.expiration > (_current_state->now() + BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) )
           FC_CAPTURE_AND_THROW( invalid_transaction_expiration, (trx_arg)(_current_state->now()) );

        auto trx_id = trx_arg.id();

        if( _current_state->is_known_transaction( trx_id ) )
           FC_CAPTURE_AND_THROW( duplicate_transaction, (trx_id) );
       
        trx = trx_arg;
        if( !_skip_signature_check )
        {
           auto digest = trx_arg.digest( _chain_id );
           for( const auto& sig : trx.signatures )
           {
              auto key = fc::ecc::public_key( sig, digest ).serialize();
              signed_keys.insert( address(key) );
              signed_keys.insert( address(pts_address(key,false,56) ) );
              signed_keys.insert( address(pts_address(key,true,56) )  );
              signed_keys.insert( address(pts_address(key,false,0) )  );
              signed_keys.insert( address(pts_address(key,true,0) )   );
           }
        }
        for( const auto& op : trx.operations )
        {
           evaluate_operation( op );
        }
        post_evaluate();
        validate_required_fee();
        update_delegate_votes();
      } 
      catch ( const fc::exception& e )
      {
         validation_error = e;
         throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx_arg) ) }

   void transaction_evaluation_state::evaluate_operation( const operation& op )
   {
      operation_factory::instance().evaluate( *this, op );
   }

   void transaction_evaluation_state::adjust_vote( slate_id_type slate_id, share_type amount )
   {
      if( slate_id )
      {
         auto slate = _current_state->get_delegate_slate( slate_id );
         if( !slate ) FC_CAPTURE_AND_THROW( unknown_delegate_slate, (slate_id) );
         for( const auto& delegate_id : slate->supported_delegates )
         {
            if( BTS_BLOCKCHAIN_ENABLE_NEGATIVE_VOTES && delegate_id < signed_int(0) ) 
               net_delegate_votes[abs(delegate_id)].votes_for -= amount;
            else
               net_delegate_votes[abs(delegate_id)].votes_for += amount;
         }
      }
   }

   share_type transaction_evaluation_state::get_fees( asset_id_type id )const
   {
      auto itr = balance.find(id);
      if( itr != balance.end() )
         return itr->second;
      return 0;
   }

   /**
    *  
    */
   void transaction_evaluation_state::sub_balance( const balance_id_type& balance_id, const asset& amount )
   {
      auto provided_deposit_itr = provided_deposits.find( balance_id );
      if( provided_deposit_itr == provided_deposits.end() )
      {
         provided_deposits[balance_id] = amount;
      }
      else
      {
         provided_deposit_itr->second += amount;
      }

      auto deposit_itr = deposits.find(amount.asset_id);
      if( deposit_itr == deposits.end()  ) 
      {
         deposits[amount.asset_id] = amount;
      }
      else
      {
         deposit_itr->second += amount;
      }

      auto balance_itr = balance.find(amount.asset_id);
      if( balance_itr != balance.end()  )
      {
          balance_itr->second -= amount.amount;
      }
      else
      {
          balance[amount.asset_id] =  -amount.amount;
      }
   }

   void transaction_evaluation_state::add_balance( const asset& amount )
   {
      auto withdraw_itr = withdraws.find( amount.asset_id );
      if( withdraw_itr == withdraws.end() )
         withdraws[amount.asset_id] = amount;
      else
         withdraw_itr->second += amount;

      auto balance_itr = balance.find( amount.asset_id );
      if( balance_itr == balance.end() )
         balance[amount.asset_id] = amount.amount;
      else
         balance_itr->second += amount.amount;
   }

   /**
    *  Throws if the asset is not known to the blockchain.
    */
   void transaction_evaluation_state::validate_asset( const asset& asset_to_validate )const
   {
      auto asset_rec = _current_state->get_asset_record( asset_to_validate.asset_id );
      if( NOT asset_rec ) 
         FC_CAPTURE_AND_THROW( unknown_asset_id, (asset_to_validate) );
   }

} } // bts::blockchain
