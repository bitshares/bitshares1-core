#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

   transaction_evaluation_state::transaction_evaluation_state( pending_chain_state* current_state )
   :_current_state( current_state )
   {
   }

   transaction_evaluation_state::~transaction_evaluation_state()
   {
   }

   bool transaction_evaluation_state::verify_authority( const multisig_meta_info& siginfo )
   { try {
      uint32_t sig_count = 0;
      ilog("@n verifying authority");
      for( const auto item : siginfo.owners )
      {
         sig_count += check_signature( item );
         ilog("@n sig_count: ${s}", ("s", sig_count));
      }
      ilog("@n required: ${s}", ("s", siginfo.required));
      return sig_count >= siginfo.required;
   } FC_CAPTURE_AND_RETHROW( (siginfo) ) }

   bool transaction_evaluation_state::check_signature( const address& a )const
   { try {
      return _skip_signature_check || signed_keys.find( a ) != signed_keys.end();
   } FC_CAPTURE_AND_RETHROW( (a) ) }

   bool transaction_evaluation_state::check_multisig( const multisig_condition& condition )const
   { try {
      auto valid = 0;
      for( auto addr : condition.owners )
          if( check_signature( addr ) )
              valid++;
      return valid >= condition.required;
   } FC_CAPTURE_AND_RETHROW( (condition) ) }

   bool transaction_evaluation_state::check_update_permission( const object_id_type id )const
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
                FC_ASSERT(false, "Unimplemenetd case in check_update_permission");
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

   void transaction_evaluation_state::update_delegate_votes()
   { try {
      for( const auto& item : delegate_vote_deltas )
      {
          const account_id_type id = item.first;
          oaccount_record delegate_record = _current_state->get_account_record( id );
          FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

          const share_type amount = item.second;
          delegate_record->adjust_votes_for( amount );
          _current_state->store_account_record( *delegate_record );
      }
   } FC_CAPTURE_AND_RETHROW() }

   void transaction_evaluation_state::validate_required_fee()
   { try {
      asset xts_fees;
      auto fee_itr = balance.find( 0 );
      if( fee_itr != balance.end() ) xts_fees += asset( fee_itr->second, 0);

      auto max_fee_itr = _max_fee.find( 0 );
      if( max_fee_itr != _max_fee.end() )
      {
         FC_ASSERT( xts_fees.amount <= max_fee_itr->second, "", ("max_fee",_max_fee)("xts_fees",xts_fees) );
      }

      xts_fees += alt_fees_paid;

      if( required_fees > xts_fees )
      {
         FC_CAPTURE_AND_THROW( insufficient_fee, (required_fees)(alt_fees_paid)(xts_fees)  );
      }
   } FC_CAPTURE_AND_RETHROW() }

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
         const oprice median_price = _current_state->get_active_feed_price( fee.first );
         if( median_price.valid() )
         {
            // fees paid in something other than XTS are discounted 50%
            alt_fees_paid += (asset( fee.second * 2, fee.first ) * *median_price) / 3;

            auto max_fee_itr = _max_fee.find( fee.first );
            if( max_fee_itr != _max_fee.end() )
            {
               FC_ASSERT( fee.second <= max_fee_itr->second );
            }
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
   } FC_CAPTURE_AND_RETHROW() }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg )
   { try {
      trx = trx_arg;
      try {
        if( _current_state->now() >= trx_arg.expiration )
        {
           const auto expired_by_sec = (_current_state->now() - trx_arg.expiration).to_seconds();
           FC_CAPTURE_AND_THROW( expired_transaction, (trx_arg)(_current_state->now())(expired_by_sec) );
        }
        if( (_current_state->now() + BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) < trx_arg.expiration )
           FC_CAPTURE_AND_THROW( invalid_transaction_expiration, (trx_arg)(_current_state->now()) );

        if( _current_state->is_known_transaction( trx_arg ) )
           FC_CAPTURE_AND_THROW( duplicate_transaction, (trx.id()) );

        if( !_skip_signature_check )
        {
           const auto trx_digest = trx_arg.digest( _current_state->get_chain_id() );
           for( const auto& sig : trx_arg.signatures )
           {
              const auto key = fc::ecc::public_key( sig, trx_digest, _enforce_canonical_signatures ).serialize();
              signed_keys.insert( address(key) );
              signed_keys.insert( address(pts_address(key,false,56) ) );
              signed_keys.insert( address(pts_address(key,true,56) )  );
              signed_keys.insert( address(pts_address(key,false,0) )  );
              signed_keys.insert( address(pts_address(key,true,0) )   );
           }
        }
        current_op_index = 0;
        for( const auto& op : trx_arg.operations )
        {
           evaluate_operation( op );
           ++current_op_index;
        }
        post_evaluate();
        validate_required_fee();
        update_delegate_votes();

        _current_state->store_transaction( trx.id(), transaction_record( transaction_location(), *this ) );
      }
      catch ( const fc::exception& e )
      {
         validation_error = e;
         throw;
      }
   } FC_CAPTURE_AND_RETHROW( (trx_arg) ) }

   void transaction_evaluation_state::evaluate_operation( const operation& op )
   { try {
      operation_factory::instance().evaluate( *this, op );
   } FC_CAPTURE_AND_RETHROW( (op) ) }

   void transaction_evaluation_state::adjust_vote( const slate_id_type slate_id, const share_type amount )
   { try {
       if( slate_id == 0 || _skip_vote_adjustment )
           return;

       const oslate_record slate_record = _current_state->get_slate_record( slate_id );
       if( !slate_record.valid() )
           FC_CAPTURE_AND_THROW( unknown_delegate_slate, (slate_id) );

       for( const account_id_type id : slate_record->slate )
       {
           if( id >= 0 )
               delegate_vote_deltas[ id ] += amount;
       }
   } FC_CAPTURE_AND_RETHROW( (slate_id)(amount) ) }

   share_type transaction_evaluation_state::get_fees( asset_id_type id )const
   { try {
      auto itr = balance.find(id);
      if( itr != balance.end() )
         return itr->second;
      return 0;
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   void transaction_evaluation_state::sub_balance( const balance_id_type& balance_id, const asset& amount )
   { try {
      balance[ amount.asset_id ] -= amount.amount;
      deposits[ amount.asset_id ] += amount.amount;
      deltas[ current_op_index ][ amount.asset_id ] += amount.amount;
   } FC_CAPTURE_AND_RETHROW( (balance_id)(amount) ) }

   void transaction_evaluation_state::add_balance( const asset& amount )
   { try {
      balance[ amount.asset_id ] += amount.amount;
      withdraws[ amount.asset_id ] += amount.amount;
      deltas[ current_op_index ][ amount.asset_id ] -= amount.amount;
   } FC_CAPTURE_AND_RETHROW( (amount) ) }

   /**
    *  Throws if the asset is not known to the blockchain.
    */
   void transaction_evaluation_state::validate_asset( const asset& asset_to_validate )const
   { try {
      auto asset_rec = _current_state->get_asset_record( asset_to_validate.asset_id );
      if( NOT asset_rec )
         FC_CAPTURE_AND_THROW( unknown_asset_id, (asset_to_validate) );
   } FC_CAPTURE_AND_RETHROW( (asset_to_validate) ) }

   bool transaction_evaluation_state::scan_deltas( const uint32_t op_index, const function<bool( const asset& )> callback )const
   { try {
       bool ret = false;
       for( const auto& item : deltas )
       {
           const uint32_t index = item.first;
           if( index != op_index ) continue;
           for( const auto& delta_item : item.second )
           {
               const asset delta_amount( delta_item.second, delta_item.first );
               ret |= callback( delta_amount );
           }
       }
       return ret;
   } FC_CAPTURE_AND_RETHROW( (op_index) ) }

   void transaction_evaluation_state::scan_addresses( const chain_interface& chain,
                                                      const function<void( const address& )> callback )const
   { try {
       for( const operation& op : trx.operations )
       {
           switch( operation_type_enum( op.type ) )
           {
               case withdraw_op_type:
               {
                   const obalance_record balance_record = chain.get_balance_record( op.as<withdraw_operation>().balance_id );
                   if( balance_record.valid() )
                   {
                       const set<address>& owners = balance_record->owners();
                       for( const address& addr : owners )
                           callback( addr );
                   }
                   break;
               }
               case deposit_op_type:
               {
                   const set<address>& owners = op.as<deposit_operation>().condition.owners();
                   for( const address& addr : owners )
                       callback( addr );
                   break;
               }
               default:
               {
                   break;
               }
           }
       }
   } FC_CAPTURE_AND_RETHROW() }

} } // bts::blockchain
