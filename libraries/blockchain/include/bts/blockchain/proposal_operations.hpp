#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/proposal_record.hpp>

namespace bts { namespace blockchain { 

   struct submit_proposal_operation
   {
      static const operation_type_enum type; 

      name_id_type          submitting_delegate_id; // the delegate_id of the submitter
      fc::time_point_sec    submission_date;
      std::string           subject;
      std::string           body;
      std::string           proposal_type; // alert, bug fix, feature upgrade, property change, etc
      fc::variant           data;  // data that is unique to the proposal

      void evaluate( transaction_evaluation_state& eval_state );
   };

   struct vote_proposal_operation
   {
      static const operation_type_enum type; 

      proposal_vote_id_type                            id;
      fc::time_point_sec                               timestamp;
      fc::enum_type<uint8_t,proposal_vote::vote_type>  vote;
      string                                           message;
     
      void evaluate( transaction_evaluation_state& eval_state );
   };

} } // bts::blockchain
FC_REFLECT( bts::blockchain::submit_proposal_operation, (submitting_delegate_id)(submission_date)(subject)(body)(proposal_type)(data) )
FC_REFLECT( bts::blockchain::vote_proposal_operation, (id)(timestamp)(vote)(message) )
