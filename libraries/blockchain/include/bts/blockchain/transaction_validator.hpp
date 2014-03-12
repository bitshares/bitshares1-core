#pragma once
#include <bts/blockchain/transaction.hpp>
#include <unordered_map>

namespace bts { namespace blockchain {
   
   class chain_database;

   class transaction_summary
   {
      public:
         transaction_summary();
         virtual ~transaction_summary(){};
         
         int64_t valid_votes;  // votes for valid blocks
         int64_t invalid_votes; // votes for invalid blocks
         int64_t fees;
         
         friend bool operator + ( const transaction_summary& a, const transaction_summary& b );
         transaction_summary& operator +=( const transaction_summary& a );
   };

   struct asset_io
   {
      asset_io():in(0),out(0),required_fees(0){}
      int64_t in;
      int64_t out;
      int64_t required_fees; ///< extra fees that are required
   };

   /** 
    * @class transaction_evaluation_state
    * @brief Tracks the state associated with the evaluation of the transaction.
    */
   class transaction_evaluation_state
   {
      public:
          transaction_evaluation_state( const signed_transaction& trx );
          virtual ~transaction_evaluation_state();
          
          int64_t  get_total_in( asset::type t = 0 )const;
          int64_t  get_total_out( asset::type t = 0 )const;
          int64_t  get_required_fees( asset::type t = 0 )const;

          void     add_required_fees( asset a );
          void     add_input_asset( asset a );
          void     add_output_asset( asset a );
          
          bool     is_output_used( uint8_t out )const;
          void     mark_output_as_used( uint8_t out );

          std::vector<meta_trx_input>               inputs;
          signed_transaction                        trx;

          bool has_signature( const address& a )const;
          bool has_signature( const pts_address& a )const;

          std::unordered_set<address>               sigs;
          std::unordered_set<pts_address>           pts_sigs;

          /** valid votes are those where one of the previous two blocks
           * is referenced by the transaction and the input 
           */
          uint64_t                                  valid_votes;
          uint64_t                                  invalid_votes;

          void balance_assets()const;
      private:
          std::unordered_map<asset::type,asset_io>  total;
          std::unordered_set<uint8_t>               used_outputs;
   };

   /**
    *  @class transaction_validator
    *  @brief base class used for evaluating transactions
    *
    *  Every unique DAC has a unique and specialized set of claim
    *  functions that require custom validation.  Validating of
    *  custom outputs requires that a lot of information be tracked
    *  to make sure value is neither created nor destroyed.
    */
   class transaction_validator
   {
       public:
          transaction_validator( chain_database* db );
          virtual ~transaction_validator();
          
          virtual transaction_summary evaluate( const signed_transaction& trx );

          virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
          virtual void validate_output( const trx_output& in, transaction_evaluation_state& state );
          
          virtual void validate_pts_signature_input( const meta_trx_input& in,
                                                     transaction_evaluation_state& state );

          virtual void validate_signature_input(  const meta_trx_input& in, 
                                                  transaction_evaluation_state& state );

          virtual void validate_signature_output( const trx_output& out, 
                                                  transaction_evaluation_state& state );
          virtual void validate_pts_signature_output( const trx_output& out, 
                                                      transaction_evaluation_state& state );

          void accumulate_votes( uint64_t amnt, uint32_t source_block_num,
                                    transaction_evaluation_state& state );
       protected:
          virtual transaction_summary on_evaluate( transaction_evaluation_state& state );
          chain_database* _db;
   };
   typedef std::shared_ptr<transaction_validator> transaction_validator_ptr;

} } // namespace bts::blockchain

FC_REFLECT( bts::blockchain::transaction_summary, (valid_votes)(invalid_votes)(fees) )
