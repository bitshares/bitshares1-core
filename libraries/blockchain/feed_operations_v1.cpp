#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

void update_feed_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
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

   if( value.is_null() )
   {
       eval_state.pending_state()->remove<feed_record>( index );
   }
   else
   {
       try
       {
           eval_state.pending_state()->store_feed_record( feed_record{ index, value.as<price>(), eval_state.pending_state()->now() } );
       }
       catch( const fc::bad_cast_exception& )
       {
       }
   }

   eval_state.pending_state()->set_market_dirty( index.quote_id, 0 );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
