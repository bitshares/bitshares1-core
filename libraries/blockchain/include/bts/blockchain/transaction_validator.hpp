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
         int64_t spent; // ignoring coindays, what is the total amount spent
         int64_t fees;
         
         friend bool operator + ( const transaction_summary& a, const transaction_summary& b );
         transaction_summary& operator +=( const transaction_summary& a );
   };


   /**
    *  @class block_evaluation_state
    *  @brief tracks state associated with evaluating blocks of transactions
    *
    *  Sometimes whether a transaction is valid or not depends upon the state of
    *  other transactions included in the same set.  This could include everything
    *  from inputs spent by other transactions to the fact that two people cannot
    *  both issue the same asset in the same block.  Thus transactions that are
    *  valid on their own may not be valid in the context of other transactions.
    *
    *  This class is designed to be derived from when extending the basic
    *  blockchain to add additional evaluation criterion. 
    */
   class block_evaluation_state
   {
      public:
         virtual ~block_evaluation_state(){}
         void add_name_output( const claim_name_output& o )
         {
            FC_ASSERT( _name_outputs.find( o.name ) == _name_outputs.end() );
            _name_outputs[o.name] = o;
         }
         void  add_input_delegate_votes( int32_t did, const asset& votes );
         void  add_output_delegate_votes( int32_t did, const asset& votes );

         std::unordered_map<std::string,claim_name_output> _name_outputs;
         std::unordered_map<int32_t,uint64_t>              _input_votes;
         std::unordered_map<int32_t,uint64_t>              _output_votes;
   };

   typedef std::shared_ptr<block_evaluation_state> block_evaluation_state_ptr;

   /** 
    * @class transaction_evaluation_state
    * @brief Tracks the state associated with the evaluation of the transaction.
    */
   class transaction_evaluation_state
   {
      public:
          struct asset_io
          {
             asset_io():in(0),out(0),required_fees(0){}
             int64_t in;
             int64_t out;
             int64_t required_fees; ///< extra fees that are required
          };

          transaction_evaluation_state( const signed_transaction& trx );
          virtual ~transaction_evaluation_state();
          
          int64_t  get_total_in( asset::unit_type t = 0 )const;
          int64_t  get_total_out( asset::unit_type t = 0 )const;
          int64_t  get_required_fees( asset::unit_type t = 0 )const;

          void     add_required_fees( asset a );
          void     add_input_asset( asset a );
          void     add_output_asset( asset a );
          void     add_name_input( const claim_name_output& o );
          bool     has_name_input( const claim_name_output& o )
          {
             return name_inputs.find(o.name) == name_inputs.end();
          }
          
          bool     is_output_used( uint32_t out )const;
          void     mark_output_as_used( uint32_t out );

          std::unordered_map<std::string,claim_name_output> name_inputs;
          std::vector<meta_trx_input>                       inputs;
          signed_transaction                                trx;

          bool has_signature( const address& a )const;
          bool has_signature( const pts_address& a )const;

          std::unordered_set<address>               sigs;
          std::unordered_set<pts_address>           pts_sigs;

          /** valid votes are those where one of the previous two blocks
           * is referenced by the transaction and the input 
           */
          uint64_t                                  valid_votes;
          uint64_t                                  invalid_votes;
          uint64_t                                  spent;


          void balance_assets()const;

      //private:
          std::unordered_map<asset::unit_type,asset_io>  total;
          std::unordered_set<uint32_t>                   used_outputs;
   };  // transaction_evaluation_state

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

          /**
           *  This method can be specialized by derived classes to track 
           *  additional state that must be validated in the context of 
           *  other transactions that are not yet part of the blockchain.
           */
          virtual block_evaluation_state_ptr create_block_state()const;
          
          virtual transaction_summary evaluate( const signed_transaction& trx, 
                                                const block_evaluation_state_ptr& block_state );

          virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state, 
                                       const block_evaluation_state_ptr& block_state );
          virtual void validate_output( const trx_output& in, transaction_evaluation_state& state, 
                                        const block_evaluation_state_ptr& block_state );
          
          virtual void validate_pts_signature_input( const meta_trx_input& in,
                                                     transaction_evaluation_state& state,
                                                     const  block_evaluation_state_ptr& block_state );

          virtual void validate_signature_input(  const meta_trx_input& in, 
                                                  transaction_evaluation_state& state, 
                                                  const block_evaluation_state_ptr& block_state );

          virtual void validate_name_input(  const meta_trx_input& in, 
                                                  transaction_evaluation_state& state, 
                                                  const block_evaluation_state_ptr& block_state );
          virtual void validate_signature_output( const trx_output& out, 
                                                  transaction_evaluation_state& state, 
                                                  const block_evaluation_state_ptr& block_state );

          virtual void validate_pts_signature_output( const trx_output& out, 
                                                      transaction_evaluation_state& state, 
                                                      const block_evaluation_state_ptr& block_state );

          virtual void validate_name_output( const trx_output& out, 
                                              transaction_evaluation_state& state, 
                                              const block_evaluation_state_ptr& block_state );
       protected:
          void accumulate_votes( uint64_t amnt, uint32_t source_block_num,
                                    transaction_evaluation_state& state );
          virtual transaction_summary on_evaluate( transaction_evaluation_state& state,
                                                   const block_evaluation_state_ptr& block_state );
          chain_database* _db;
   };
   typedef std::shared_ptr<transaction_validator> transaction_validator_ptr;

} } // namespace bts::blockchain

FC_REFLECT( bts::blockchain::transaction_summary, (valid_votes)(invalid_votes)(fees) )

FC_REFLECT( bts::blockchain::transaction_evaluation_state::asset_io, (in)(out)(required_fees) )
FC_REFLECT( bts::blockchain::transaction_evaluation_state, (name_inputs)(inputs)(trx)(sigs)
                                                          (pts_sigs)(valid_votes)(invalid_votes)(spent)(used_outputs)(total) )
