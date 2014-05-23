#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {
   struct proposal_record
   {
      bool is_null()const { return submitting_delegate_id == -1; }
      proposal_record make_null()const    { proposal_record cpy(*this); cpy.submitting_delegate_id = -1; return cpy; }

      proposal_id_type      id;
      name_id_type          submitting_delegate_id; // the delegate_id of the submitter
      fc::time_point_sec    submission_date;
      std::string           subject;
      std::string           body;
      std::string           proposal_type; // alert, bug fix, feature upgrade, property change, etc
      fc::variant           data;  // data that is unique to the proposal
   };
   typedef fc::optional<proposal_record> oproposal_record;
   
   struct proposal_vote
   {
      enum vote_type 
      {
          no  = 0,
          yes = 1,
          undefined = 2
      };
      bool is_null()const { return vote == undefined; }
      proposal_vote make_null()const { proposal_vote cpy(*this); cpy.vote = undefined; return cpy; }

      proposal_vote_id_type             id;
      fc::time_point_sec                timestamp;
      fc::enum_type<uint8_t,vote_type>  vote;
   };
   typedef fc::optional<proposal_vote> oproposal_vote;
}} // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::proposal_vote::vote_type, (no)(yes)(undefined) )
FC_REFLECT( bts::blockchain::proposal_vote, (id)(timestamp)(vote) )
FC_REFLECT( bts::blockchain::proposal_record, (id)(submitting_delegate_id)(submission_date)(subject)(body)(proposal_type)(data) )
