#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   bool transaction_evaluation_state::verify_authority( const multisig_meta_info& siginfo )const
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
      return _skip_signature_check || signed_addresses.find( a ) != signed_addresses.end();
   } FC_CAPTURE_AND_RETHROW( (a) ) }

   bool transaction_evaluation_state::check_multisig( const multisig_condition& condition )const
   { try {
      auto valid = 0;
      for( auto addr : condition.owners )
          if( check_signature( addr ) )
              valid++;
      return valid >= condition.required;
   } FC_CAPTURE_AND_RETHROW( (condition) ) }

   bool transaction_evaluation_state::any_parent_has_signed( const string& account_name )const
   { try {
       for( optional<string> parent_name = pending_state()->get_parent_account_name( account_name );
            parent_name.valid();
            parent_name = pending_state()->get_parent_account_name( *parent_name ) )
       {
           const oaccount_record parent_record = pending_state()->get_account_record( *parent_name );
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
          oaccount_record delegate_record = pending_state()->get_account_record( id );
          FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

          const share_type amount = item.second;
          delegate_record->adjust_votes_for( amount );
          pending_state()->store_account_record( *delegate_record );
      }
   } FC_CAPTURE_AND_RETHROW() }

   void transaction_evaluation_state::validate_fees()
   { try {
       for( const auto& item : fees_paid )
       {
           const asset fee( item.second, item.first );
           if( fee.amount < 0 )
               FC_CAPTURE_AND_THROW( negative_fee, (fee) );

           auto iter = min_fees.find( fee.asset_id );
           if( iter != min_fees.end() )
           {
               if( fee.amount < iter->second )
                   FC_CAPTURE_AND_THROW( insufficient_fee, (fee)(min_fees) );
           }

           iter = max_fees.find( fee.asset_id );
           if( iter != max_fees.end() )
           {
               if( fee.amount > iter->second )
                   FC_CAPTURE_AND_THROW( fee_greater_than_max, (fee)(max_fees) );
           }

           if( fee.amount != 0 )
           {
               oasset_record asset_record = pending_state()->get_asset_record( fee.asset_id );
               if( !asset_record.valid() )
                   FC_CAPTURE_AND_THROW( unknown_asset_id, (fee) );

               asset_record->collected_fees += fee.amount;
               pending_state()->store_asset_record( *asset_record );
           }
       }

       if( pending_state()->get_head_block_num() >= BTS_V0_9_2_FORK_BLOCK_NUM )
       {
           for( const auto& op : trx.operations )
           {
               if( operation_type_enum( op.type ) == cover_op_type )
               {
                   FC_ASSERT( fees_paid[ 0 ] <= BTS_BLOCKCHAIN_PRECISION );
               }
           }
       }
   } FC_CAPTURE_AND_RETHROW() }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg )
   { try {
      trx = trx_arg;
      try {
        if( pending_state()->now() >= trx_arg.expiration )
        {
           if( pending_state()->now() > trx_arg.expiration || pending_state()->get_head_block_num() >= BTS_V0_4_21_FORK_BLOCK_NUM )
           {
               const auto expired_by_sec = (pending_state()->now() - trx_arg.expiration).to_seconds();
               FC_CAPTURE_AND_THROW( expired_transaction, (trx_arg)(pending_state()->now())(expired_by_sec) );
           }
        }
        if( (pending_state()->now() + BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) < trx_arg.expiration )
           FC_CAPTURE_AND_THROW( invalid_transaction_expiration, (trx_arg)(pending_state()->now()) );

        if( pending_state()->get_head_block_num() >= BTS_V0_4_26_FORK_BLOCK_NUM )
        {
            if( pending_state()->is_known_transaction( trx_arg ) )
               FC_CAPTURE_AND_THROW( duplicate_transaction, (trx.id()) );
        }

        if( !_skip_signature_check )
        {
           const auto trx_digest = trx_arg.digest( pending_state()->get_chain_id() );
           for( const auto& sig : trx_arg.signatures )
           {
              const auto key = fc::ecc::public_key( sig, trx_digest, _enforce_canonical_signatures ).serialize();
              signed_addresses.insert( address( key ) );
              signed_addresses.insert( address( pts_address( key, false, 56 ) ) );
              signed_addresses.insert( address( pts_address( key, true, 56 ) ) );
              signed_addresses.insert( address( pts_address( key, false, 0 ) ) );
              signed_addresses.insert( address( pts_address( key, true, 0 ) ) );
           }
        }

        _current_op_index = 0;
        for( const auto& op : trx_arg.operations )
        {
           evaluate_operation( op );
           ++_current_op_index;
        }

        validate_fees();
        calculate_base_equivalent_fees();
        update_delegate_votes();

        pending_state()->store_transaction( trx.id(), transaction_record( transaction_location(), *this ) );
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

       const oslate_record slate_record = pending_state()->get_slate_record( slate_id );
       if( !slate_record.valid() )
           FC_CAPTURE_AND_THROW( unknown_delegate_slate, (slate_id) );

       if( slate_record->duplicate_slate.empty() )
       {
           for( const account_id_type id : slate_record->slate )
           {
               if( id >= 0 )
                   delegate_vote_deltas[ id ] += amount;
           }
       }
       else
       {
           for( const account_id_type id : slate_record->duplicate_slate )
               delegate_vote_deltas[ id ] += amount;
       }
   } FC_CAPTURE_AND_RETHROW( (slate_id)(amount) ) }

   void transaction_evaluation_state::calculate_base_equivalent_fees()
   { try {
       total_base_equivalent_fees_paid = 0;

       for( const auto& item : fees_paid )
       {
           const asset fee( item.second, item.first );
           if( fee.asset_id == 0 )
           {
               total_base_equivalent_fees_paid += fee.amount;
           }
           else
           {
               const oasset_record asset_record = pending_state()->get_asset_record( fee.asset_id );
               FC_ASSERT( asset_record.valid() );

               // Tally BitAsset fees using feed price discounted by 33%
               if( asset_record->is_market_issued() )
               {
                   const oprice feed_price = pending_state()->get_active_feed_price( fee.asset_id );
                   if( feed_price.valid() )
                       total_base_equivalent_fees_paid += (asset( fee.amount * 2, fee.asset_id ) * *feed_price).amount / 3;
               }
           }
       }
   } FC_CAPTURE_AND_RETHROW() }

   void transaction_evaluation_state::sub_balance( const asset& amount )
   { try {
      fees_paid[ amount.asset_id ] -= amount.amount;
      op_deltas[ _current_op_index ][ amount.asset_id ] += amount.amount;
   } FC_CAPTURE_AND_RETHROW( (amount) ) }

   void transaction_evaluation_state::add_balance( const asset& amount )
   { try {
      fees_paid[ amount.asset_id ] += amount.amount;
      op_deltas[ _current_op_index ][ amount.asset_id ] -= amount.amount;
   } FC_CAPTURE_AND_RETHROW( (amount) ) }

   bool transaction_evaluation_state::scan_op_deltas( const uint32_t op_index, const function<bool( const asset& )> callback )const
   { try {
       bool ret = false;
       for( const auto& item : op_deltas )
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
       set<address> addrs;

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
                           addrs.insert( addr );
                   }
                   break;
               }
               case deposit_op_type:
               {
                   const set<address>& owners = op.as<deposit_operation>().condition.owners();
                   for( const address& addr : owners )
                       addrs.insert( addr );
                   break;
               }
               case register_account_op_type:
               {
                   const register_account_operation account_register = op.as<register_account_operation>();
                   addrs.emplace( account_register.owner_key );
                   addrs.emplace( account_register.active_key );
                   break;
               }
               case update_account_op_type:
               {
                   const update_account_operation account_update = op.as<update_account_operation>();
                   if( account_update.active_key.valid() )
                       addrs.emplace( *account_update.active_key );
                   break;
               }
               case bid_op_type:
               {
                   addrs.insert( op.as<bid_operation>().bid_index.owner );
                   break;
               }
               case ask_op_type:
               {
                   addrs.insert( op.as<ask_operation>().ask_index.owner );
                   break;
               }
               case short_op_v2_type:
               {
                   addrs.insert( op.as<short_operation>().short_index.owner );
                   break;
               }
               case short_op_type:
               {
                   addrs.insert( op.as<short_operation_v1>().short_index.owner );
                   break;
               }
               case cover_op_type:
               {
                   addrs.insert( op.as<cover_operation>().cover_index.owner );
                   break;
               }
               case add_collateral_op_type:
               {
                   addrs.insert( op.as<add_collateral_operation>().cover_index.owner );
                   break;
               }
               default:
               {
                   break;
               }
           }
       }

       for( const address& addr : addrs )
           callback( addr );
   } FC_CAPTURE_AND_RETHROW() }

} } // bts::blockchain
