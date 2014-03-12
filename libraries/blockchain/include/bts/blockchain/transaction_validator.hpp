#pragma once
#include <bts/blockchain/transaction.hpp>
#include <unordered_map>

namespace bts { namespace blockchain {
   
   class chain_database;

   class transaction_summary
   {
      public:
         virtual ~transaction_summary();
         
         asset  valid_votes;  // votes for valid blocks
         asset  invalid_votes; // votes for invalid blocks
         asset  fees;
         
         friend bool operator + ( const transaction_summary& a, const transaction_summary& b );
         transaction_summary& operator +=( const transaction_summary& a );
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
          
          uint64_t get_total_in( asset::type t )const;
          uint64_t get_total_out( asset::type t )const;
          void     add_input_asset( asset a );
          void     add_output_asset( asset a );
          
          bool     is_output_used( uint8_t out )const;
          void     mark_output_as_used( uint8_t out );

          std::vector<meta_trx_input>               inputs;
          signed_transaction                        trx;
      private:
          std::unordered_map<asset::type,uint64_t>  total_in;
          std::unordered_map<asset::type,uint64_t>  total_out;
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

       protected:
          virtual transaction_summary on_evaluate( transaction_evaluation_state& state );
       private:

          chain_database* _db;
   };
   typedef std::shared_ptr<transaction_validator> transaction_validator_ptr;

} } // namespace bts::blockchain
