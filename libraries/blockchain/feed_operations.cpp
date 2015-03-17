#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   void update_feed_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( eval_state.pending_state()->get_head_block_num() < BTS_V0_5_0_FORK_BLOCK_NUM )
          return evaluate_v1( eval_state );

      const oaccount_record account_record = eval_state.pending_state()->get_account_record( index.delegate_id );
      if( !account_record.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (index.delegate_id) );

      if( account_record->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (*account_record) );

      if( !account_record->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (*account_record) );

      if( !eval_state.check_signature( account_record->signing_address() )
          && !eval_state.account_or_any_parent_has_signed( *account_record ) )
      {
          FC_CAPTURE_AND_THROW( missing_signature, (*this) );
      }

      price feed_price;
      try
      {
          feed_price = value.as<price>();
      }
      catch( const fc::bad_cast_exception& )
      {
          FC_CAPTURE_AND_THROW( invalid_feed_price, (value) );
      }

      if( feed_price.quote_asset_id != index.quote_id || feed_price.base_asset_id != asset_id_type( 0 ) )
          FC_CAPTURE_AND_THROW( invalid_feed_price, (feed_price) );

      const oasset_record asset_record = eval_state.pending_state()->get_asset_record( index.quote_id );
      if( !asset_record.valid() )
          FC_CAPTURE_AND_THROW( unknown_asset_id, (index.quote_id) );

      if( !asset_record->is_market_issued() )
          FC_CAPTURE_AND_THROW( invalid_market, (index.quote_id) );

      eval_state.pending_state()->store_feed_record( feed_record{ index, feed_price, eval_state.pending_state()->now() } );
      eval_state.pending_state()->set_market_dirty( feed_price.quote_asset_id, feed_price.base_asset_id );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
