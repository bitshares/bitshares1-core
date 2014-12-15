#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

   transaction_evaluation_state::transaction_evaluation_state( chain_interface* current_state, digest_type chain_id )
   :_current_state( current_state ),_chain_id(chain_id),_skip_signature_check(false)
   {
   }

   transaction_evaluation_state::~transaction_evaluation_state()
   {
   }

   bool transaction_evaluation_state::verify_authority( const multisig_meta_info& siginfo )
   {
      uint32_t sig_count = 0;
      ilog("@n verifying authority");
      for( const auto item : siginfo.owners )
      {
         sig_count += check_signature( item );
         ilog("@n sig_count: ${s}", ("s", sig_count));
      }
      ilog("@n required: ${s}", ("s", siginfo.required));
      return sig_count >= siginfo.required;
   }

   bool transaction_evaluation_state::check_signature( const address& a )const
   { try {
      return  _skip_signature_check || signed_keys.find( a ) != signed_keys.end();
   } FC_CAPTURE_AND_RETHROW( (a) ) }

   bool transaction_evaluation_state::check_multisig( const multisig_condition& condition )const
   { try {

      auto valid = 0;
      for( auto addr : condition.owners )
          if( check_signature( addr ) )
              valid++;
      return valid >= condition.required;

   } FC_CAPTURE_AND_RETHROW( (condition) ) }


   bool transaction_evaluation_state::check_update_permission( const object_id_type& id )const
   { try {
        if( _skip_signature_check )
            return true;
        auto object = _current_state->get_object_record( id );
        FC_ASSERT( object.valid(), "Checking update permission for an object that doesn't exist!");
        switch( object->type() )
        {
            case( obj_type::base_object ):
            {
                return check_multisig( object->_owners );
                break;
            }
            default:
                FC_ASSERT(!"Unimplemenetd case in check_update_permission");
        }
        return false;
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   bool transaction_evaluation_state::any_parent_has_signed( const string& account_name )const
   { try {
       for( optional<string> parent_name = _current_state->get_parent_account_name( account_name );
            parent_name.valid();
            parent_name = _current_state->get_parent_account_name( *parent_name ) )
       {
           const oaccount_record parent_record = _current_state->get_account_record( *parent_name );
           if( !parent_record.valid() )
               continue;

           if( parent_record->is_retracted() )
               continue;

           if( check_signature( parent_record->active_key() ) )
               return true;

           if( check_signature( parent_record->owner_key ) )
               return true;
       }
       return false;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   bool transaction_evaluation_state::account_or_any_parent_has_signed( const account_record& record )const
   { try {
       if( !record.is_retracted() )
       {
           if( check_signature( record.active_key() ) )
               return true;

           if( check_signature( record.owner_key ) )
               return true;
       }
       return any_parent_has_signed( record.name );
   } FC_CAPTURE_AND_RETHROW( (record) ) }

   void transaction_evaluation_state::verify_delegate_id( account_id_type id )const
   {
      auto current_account = _current_state->get_account_record( id );
      if( !current_account ) FC_CAPTURE_AND_THROW( unknown_account_id, (id) );
      if( !current_account->is_delegate() ) FC_CAPTURE_AND_THROW( not_a_delegate, (id) );
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
      for( const auto& item : withdraws )
      {
         auto asset_rec = _current_state->get_asset_record( item.first );
         if( !asset_rec.valid() ) FC_CAPTURE_AND_THROW( unknown_asset_id, (item) );
         if( asset_rec->id > 0 && asset_rec->is_market_issued() && asset_rec->transaction_fee > 0 )
         {
            sub_balance( address(), asset(asset_rec->transaction_fee, asset_rec->id) );
         }
      }

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
         oprice median_price = _current_state->get_median_delegate_price( fee.first, asset_id_type( 0 ) );
         if( median_price )
         {
            // fees paid in something other than XTS are discounted 50%
            alt_fees_paid += asset( (fee.second*2)/3, fee.first ) * *median_price;
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
            _current_state->store_asset_record( *asset_record );
         }
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg, bool skip_signature_check, bool enforce_canonical )
   { try {
      _skip_signature_check = skip_signature_check;
      try {
        if( _current_state->now() >= trx_arg.expiration )
        {
           const auto expired_by_sec = (_current_state->now() - trx_arg.expiration).to_seconds();
           FC_CAPTURE_AND_THROW( expired_transaction, (trx_arg)(_current_state->now())(expired_by_sec) );
        }
        if( (_current_state->now() + BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) < trx_arg.expiration )
           FC_CAPTURE_AND_THROW( invalid_transaction_expiration, (trx_arg)(_current_state->now()) );

        auto trx_id = trx_arg.id();

        if( _current_state->is_known_transaction( trx_arg.expiration, trx_arg.digest( _chain_id ) ) )
           FC_CAPTURE_AND_THROW( duplicate_transaction, (trx_id) );

        trx = trx_arg;
        if( !_skip_signature_check )
        {
           auto digest = trx_arg.digest( _chain_id );
           for( const auto& sig : trx.signatures )
           {
              auto key = fc::ecc::public_key( sig, digest, enforce_canonical ).serialize();
              signed_keys.insert( address(key) );
              signed_keys.insert( address(pts_address(key,false,56) ) );
              signed_keys.insert( address(pts_address(key,true,56) )  );
              signed_keys.insert( address(pts_address(key,false,0) )  );
              signed_keys.insert( address(pts_address(key,true,0) )   );
           }
        }
        current_op_index = 0;
        for( const auto& op : trx.operations )
        {
           evaluate_operation( op );
           ++current_op_index;
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

   void transaction_evaluation_state::sub_balance( const balance_id_type& balance_id, const asset& amount )
   { try {
      if( balance_id != balance_id_type() )
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
          balance[amount.asset_id] = -amount.amount;
      }

      deltas[ current_op_index ] = amount;
   } FC_CAPTURE_AND_RETHROW( (balance_id)(amount) ) }

   void transaction_evaluation_state::add_balance( const asset& amount )
   { try {
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

      deltas[ current_op_index ] = -amount;
   } FC_CAPTURE_AND_RETHROW( (amount) ) }

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
