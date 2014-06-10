#include <bts/blockchain/proposal_operations.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {
   void submit_proposal_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       auto delegate_record = eval_state._current_state->get_account_record( submitting_delegate_id );
       if( !delegate_record ) 
          FC_CAPTURE_AND_THROW( unknown_account_id, (submitting_delegate_id) );

       if( !delegate_record->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (delegate_record) );

       proposal_record new_proposal;
       new_proposal.id = eval_state._current_state->new_proposal_id();
       new_proposal.submitting_delegate_id = this->submitting_delegate_id;
       new_proposal.submission_date = this->submission_date;
       new_proposal.subject = this->subject;
       new_proposal.body = this->body;
       new_proposal.proposal_type = this->proposal_type;
       new_proposal.data = this->data;

       if( !eval_state.check_signature(delegate_record->active_address()) )
          FC_CAPTURE_AND_THROW( missing_signature, (delegate_record->active_key()) );

       eval_state._current_state->store_proposal_record( new_proposal );

   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void vote_proposal_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       auto vote_message_size = this->message.size();
       if( vote_message_size > BTS_BLOCKCHAIN_PROPOSAL_VOTE_MESSAGE_MAX_SIZE )
          FC_CAPTURE_AND_THROW( invalid_vote_message_size, (vote_message_size) );

       ///  signed by a registered delegate
       auto delegate_id = id.delegate_id;
       auto delegate_record = eval_state._current_state->get_account_record( delegate_id );
       if( !delegate_record ) 
          FC_CAPTURE_AND_THROW( unknown_account_id, (delegate_id) );

       if( !delegate_record->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (delegate_record) );

       if( !eval_state._current_state->is_active_delegate(delegate_id)  )
          FC_CAPTURE_AND_THROW( not_an_active_delegate, (delegate_record) );

       if( !eval_state.check_signature(delegate_record->active_address()) )
          FC_CAPTURE_AND_THROW( missing_signature, (delegate_record->active_key()) );

       auto proposal_record = eval_state._current_state->get_proposal_record( id.proposal_id );
       if( !proposal_record )
          FC_CAPTURE_AND_THROW( unknown_proposal_id, (id.proposal_id) );

       proposal_vote new_vote;
       new_vote.id = this->id;
       new_vote.timestamp = this->timestamp;
       new_vote.vote = this->vote;
       new_vote.message = this->message;

       // TODO:  check to see if there are enough votes on this proposal to 'ratify it' / lock it in.

       eval_state._current_state->store_proposal_vote( new_vote );

   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } 
