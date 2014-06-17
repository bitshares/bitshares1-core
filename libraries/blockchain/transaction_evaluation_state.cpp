#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/operation_factory.hpp>

namespace bts { namespace blockchain { 

   transaction_evaluation_state::transaction_evaluation_state( const chain_interface_ptr& current_state, digest_type chain_id )
   :_current_state( current_state ),_chain_id(chain_id)
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
      return  signed_keys.find( a ) != signed_keys.end();
   }


   void transaction_evaluation_state::add_required_deposit( const address& owner_key, const asset& amount )
   {
      FC_ASSERT( !!trx.delegate_id );
      balance_id_type balance_id = withdraw_condition( 
                                       withdraw_with_signature( owner_key ), 
                                       amount.asset_id, *trx.delegate_id ).get_address();

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
      auto asset_rec = _current_state->get_asset_record( BASE_ASSET_ID );
      auto max_votes = 2 * (asset_rec->current_share_supply / BTS_BLOCKCHAIN_NUM_DELEGATES);

      for( auto del_vote : net_delegate_votes )
      {
         auto del_rec = _current_state->get_account_record( del_vote.first );
         FC_ASSERT( !!del_rec );
         del_rec->adjust_votes_for( del_vote.second.votes_for );
         del_rec->adjust_votes_against( del_vote.second.votes_against );

         if( del_rec->votes_for() > max_votes || del_rec->votes_against() > max_votes )
            FC_CAPTURE_AND_THROW( delegate_vote_limit, (del_rec)(max_votes) );

         _current_state->store_account_record( *del_rec );
      }
   }

   void transaction_evaluation_state::validate_required_fee()
   { try {
      share_type required_fee = _current_state->calculate_data_fee( trx.data_size() );
      auto fee_itr = balance.find( 0 );
      if( fee_itr == balance.end() ||
          fee_itr->second < required_fee ) 
      {
         FC_CAPTURE_AND_THROW( insufficient_fee, (required_fee) );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    *  Process all fees and update the asset records.
    */
   void transaction_evaluation_state::post_evaluate()
   { try {
      required_fees += asset(_current_state->calculate_data_fee(fc::raw::pack_size(trx)),0);
      for( auto fee : balance )
      {
         if( fee.second < 0 ) FC_CAPTURE_AND_THROW( negative_fee, (fee) );
         if( fee.second > 0 )
         {
            if( fee.first == 0 && fee.second < required_fees.amount )
               FC_CAPTURE_AND_THROW( insufficient_fee, (fee)(required_fees.amount) );

            auto asset_record = _current_state->get_asset_record( fee.first );
            if( !asset_record )
              FC_CAPTURE_AND_THROW( unknown_asset_id, (fee.first) );

            asset_record->collected_fees += fee.second;
            asset_record->current_share_supply -= fee.second;
            _current_state->store_asset_record( *asset_record );
         }
      }
      for( auto required_deposit : required_deposits )
      {
         auto provided_itr = provided_deposits.find( required_deposit.first );
         
         if( provided_itr->second < required_deposit.second )
            FC_CAPTURE_AND_THROW( missing_deposit, (required_deposit) );
      }

   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg )
   { try {
      reset();
      try {
        if( trx_arg.expiration && *trx_arg.expiration < _current_state->now() )
           FC_CAPTURE_AND_THROW( expired_transaction, (trx_arg)(_current_state->now()) );
       
        auto trx_id = trx_arg.id();
        otransaction_record known_transaction= _current_state->get_transaction( trx_id );
        if( known_transaction )
           FC_CAPTURE_AND_THROW( duplicate_transaction, (known_transaction) );
       
        trx = trx_arg;
        auto digest = trx_arg.digest( _chain_id );
        for( auto sig : trx.signatures )
        {
           auto key = fc::ecc::public_key( sig, digest ).serialize();
           signed_keys.insert( address(key) );
           signed_keys.insert( address(pts_address(key,false,56) ) );
           signed_keys.insert( address(pts_address(key,true,56) )  );
           signed_keys.insert( address(pts_address(key,false,0) )  );
           signed_keys.insert( address(pts_address(key,true,0) )   );
        }
        for( auto op : trx.operations )
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
   void transaction_evaluation_state::add_vote( account_id_type delegate_id, share_type amount )
   {
      auto account_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[account_id].votes_for += amount;
      else if( delegate_id < 0 )
         net_delegate_votes[account_id].votes_against += amount;
   }
   void transaction_evaluation_state::sub_vote( account_id_type delegate_id, share_type amount )
   {
      auto account_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[account_id].votes_for -= amount;
      else if( delegate_id < 0 )
         net_delegate_votes[account_id].votes_against -= amount;
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


} } 

