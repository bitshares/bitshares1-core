#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/blockchain_db.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
  
    /**
     *  While validating a transaction certain things need to be tracked to make
     *  sure the transaction is valid.  Thus struct tracks this information.  
     *
     *  This state assumes the meta_trx_input is accurate.
     *
     *  TODO: add current bid/ask spread for all pairs in the block chain as this
     *        information is required for margin call inputs.
     */
    class trx_validation_state
    {
        public:
           /**
            * @param t - the transaction that is being validated
            *
            * @param head_idx - the head index to evaluate this
            * transaction against.  This should be the prior block
            * before the one t will be included in.
            */
           trx_validation_state( const signed_transaction& t, 
                                blockchain_db* d, 
                                bool enforce_unspent_in = true,
                                uint32_t  head_idx = -1
                                );
           bool allow_short_long_matching;

           trx_validation_state() : trx(signed_transaction()) {}
           
           /** tracks the sum of all inputs and outputs for a particular
            * asset type in the balance_sheet 
            */
           struct asset_balance
           {
              asset in;
              asset collat_in;
              asset neg_in; 
              asset out;
              asset collat_out;
              asset neg_out; // collateral
              bool is_balanced()const 
              { 
                 int64_t sum = int64_t(in.amount.high_bits());
                 sum -= int64_t(neg_in.amount.high_bits());
                 sum -= int64_t(out.amount.high_bits());
                 sum += int64_t(neg_out.amount.high_bits());
                 ilog( "is blanced sum ${s} ${u}", ("s",sum)("u",in.unit));
                 return abs(sum) <= 2;
              }
              bool creates_money()const 
              { 
                 int64_t sum = int64_t(in.amount.high_bits());
                 sum -= int64_t(neg_in.amount.high_bits());
                 sum -= int64_t(out.amount.high_bits());
                 sum += int64_t(neg_out.amount.high_bits());
                 ilog( "is blanced sum ${s} ${u}", ("s",sum)("u",in.unit));
                 if(  abs(sum) <= 2 ) return false;
                 return sum < 0;
             }

                 //return ((in - neg_in) - (out - neg_out)).amount >= fc::uint128(0); }
           };

           /** this is cost because validation shouldn't modify the trx and
            * is captured by value rather than reference just in case we need 
            * thread safety of some kind... for performance see if we can
            * capture it by reference later.
            */
           const signed_transaction            trx; // TODO make reference?
           uint64_t total_cdd;
           uint64_t uncounted_cdd;

           uint32_t prev_block_id1; // block ids that count for CDD
           uint32_t prev_block_id2; // block ids that count for CDD
           std::vector<meta_trx_input>         inputs;
                                             
           std::vector<asset_balance>          balance_sheet; // validate 0 sum 
       //    std::vector<asset_issuance>         issue_sheet; // update backing info


           /** spending of some inputs depends upon the existance of outputs to the
            * transaction that meet certain conditions such as exchange rates. The
            * same output cannot satisify 2 different input requirements. As we
            * process inputs we track which outputs have been used and make sure
            * there are no duplicates.
            */
           std::unordered_set<uint8_t>         used_outputs;
           std::unordered_set<address>         signed_addresses;

           /**
            *  contains all addresses for which a signature is required,
            *  this is validated last with the exception of multi-sig or
            *  escrow inputs which have optional signatures.
            */
           std::unordered_set<address>         required_sigs;

           /** dividends earned in the past 100 blocks that are counted toward
             * transaction fees.
             */
           asset                               dividend_fees;
           asset                               dividends;


           blockchain_db*                      db;

           bool                                enforce_unspent;
           uint32_t                            ref_head;
           /** @throw an exception on error */
           void validate();
        private:
           static const uint16_t output_not_found = uint16_t(-1);
           void     mark_output_as_used( uint16_t output_number );
           uint16_t find_unused_sig_output( const address& a, const asset& bal );
           uint16_t find_unused_bid_output( const claim_by_bid_output& b );
           uint16_t find_unused_long_output( const claim_by_long_output& b );
           uint16_t find_unused_cover_output( const claim_by_cover_output& b, uint64_t min_collat );

           void validate_input( const meta_trx_input& );
           void validate_signature( const meta_trx_input& );
           void validate_pts( const meta_trx_input& );
           void validate_bid( const meta_trx_input& );
           void validate_long( const meta_trx_input& );
           void validate_cover( const meta_trx_input& );
           void validate_opt( const meta_trx_input& );
           void validate_multi_sig( const meta_trx_input& );
           void validate_escrow( const meta_trx_input& );
           void validate_password( const meta_trx_input& );

           void validate_output( const trx_output& );
           void validate_pts( const trx_output& );
           void validate_signature( const trx_output& );
           void validate_bid( const trx_output& );
           void validate_long( const trx_output& );
           void validate_cover( const trx_output& );
           void validate_opt( const trx_output& );
           void validate_multi_sig( const trx_output& );
           void validate_escrow( const trx_output& );
           void validate_password( const trx_output& );
    };

} } // bts::blockchain
FC_REFLECT( bts::blockchain::trx_validation_state::asset_balance, (in)(neg_in)(collat_in)(out)(neg_out)(collat_out) )
FC_REFLECT( bts::blockchain::trx_validation_state, 
    (trx)
    (inputs)
    (ref_head)
    (balance_sheet)
    (used_outputs)
    (required_sigs)
    (signed_addresses)
    (dividend_fees)
)
