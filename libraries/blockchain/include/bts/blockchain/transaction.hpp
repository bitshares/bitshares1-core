#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/error_codes.hpp>
#include <fc/optional.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>

#include <unordered_set>

namespace bts { namespace blockchain {

   class chain_interface;
   typedef std::shared_ptr<chain_interface> chain_interface_ptr;

   /**
    *  A transaction is a set of operations that are
    *  performed atomicly and must be internally consistant.
    *
    *  Every transaction votes for 
    */
   struct transaction
   {
      transaction(){}

      digest_type                      digest()const;

      fc::optional<fc::time_point_sec> expiration;
      fc::optional<name_id_type>       delegate_id; // delegate being voted for in required payouts
      std::vector<operation>           operations; 

      void withdraw( const balance_id_type& account, share_type amount );
      void deposit( const address& addr, const asset& amount, name_id_type delegate_id );
      void reserve_name( const std::string& name, const std::string& json_data, const public_key_type& master, const public_key_type& active, bool as_delegate = false );
      void update_name( name_id_type name_id, const fc::optional<std::string>& json_data, const fc::optional<public_key_type>& active, bool as_delegate = false );
   };
   struct transaction_summary_details
   {
      /**
       *  Bitcoin compatibility 
       */
      ///@{ 
        std::string        account;
        std::string        category;
        std::string        address; 
        share_type         amount; 
      ///@}
        asset_id_type      asset_id;
   };

   struct transaction_summary
   {
      transaction_summary():amount(0),confirmations(0),blockindex(0){}

      /**
       *  Bitcoin compatibility 
       */
      ///@{ 
      share_type                                 amount;
      uint32_t                                   confirmations;
      block_id_type                              blockhash;
      uint32_t                                   blockindex;
      fc::time_point_sec                         blocktime;
      transaction_id_type                        txid;
      fc::time_point_sec                         time;
      fc::time_point_sec                         timereceived;
      std::vector<transaction_summary_details>   details;
      std::vector<char>                          hex;
      ///@}

      std::vector<asset>                         fees;
      std::vector<asset>                         amounts;
      fc::variant                                json_data;
   };


   struct signed_transaction : public transaction
   {
      transaction_id_type                     id()const;
      size_t                                  data_size()const;
      void                                    sign( const fc::ecc::private_key& signer );

      std::vector<fc::ecc::compact_signature> signatures;
   };
   typedef std::vector<signed_transaction> signed_transactions;
   typedef fc::optional<signed_transaction> osigned_transaction;

   /**
    *  While evaluating a transaction there is a lot of intermediate
    *  state that must be tracked.  Any shares withdrawn from the
    *  database must be stored in the transaction state until they
    *  are sent back to the database as either new balances or
    *  as fees collected.
    *
    *  Some outputs such as markets, options, etc require certain
    *  payments to be made.  So payments made are tracked and
    *  compared against payments required.
    *
    */
   class transaction_evaluation_state
   {
      public:
         transaction_evaluation_state( const chain_interface_ptr& blockchain );
         transaction_evaluation_state(){};

         virtual ~transaction_evaluation_state();
         virtual share_type get_fees( asset_id_type id = 0)const;

         virtual void reset();
         
         virtual void evaluate( const signed_transaction& trx );
         virtual void evaluate_operation( const operation& op );

         /** perform any final operations based upon the current state of 
          * the operation such as updating fees paid etc.
          */
         virtual void post_evaluate();
         /** can be specalized to handle many different forms of
          * fee payment.
          */
         virtual void validate_required_fee();
         /**
          * apply collected vote changes 
          */
         virtual void update_delegate_votes();
         
         virtual void evaluate_withdraw( const withdraw_operation& op );
         virtual void evaluate_deposit( const deposit_operation& op );
         virtual void evaluate_reserve_name( const reserve_name_operation& op );
         virtual void evaluate_update_name( const update_name_operation& op );
         virtual void evaluate_create_asset( const create_asset_operation& op );
         virtual void evaluate_update_asset( const update_asset_operation& op );
         virtual void evaluate_issue_asset( const issue_asset_operation& op );
         
         virtual void fail( bts_error_code error_code, const fc::variant& data );
         
         bool check_signature( const address& a )const;
         void add_required_signature( const address& a );
         
         // steps performed as the transaction is validated
         
        
         /**
          *  subtracts amount from a withdraw_with_signature account with the
          *  owner_key and amount.asset_id and the delegate_id of the transaction.
          */
         void add_required_deposit( const address& owner_key, const asset& amount );
         
         /** contains address funds were deposited into for use in
          * incrementing required_deposits balance
          */
         void sub_balance( const balance_id_type& addr, const asset& amount );
         void add_balance( const asset& amount );
         
         /** any time a balance is deposited increment the vote for the delegate,
          * if delegate_id then it is a vote against abs(delegate_id)
          */
         void add_vote( name_id_type delegate_id, share_type amount );
         void sub_vote( name_id_type delegate_id, share_type amount );
         
         signed_transaction                               trx;
         std::unordered_set<address>                      signed_keys;
         std::unordered_set<address>                      required_keys;
         
         // increases with funds are withdrawn, decreases when funds are deposited or fees paid
         uint32_t                                         validation_error_code;
         fc::variant                                      validation_error_data;
         
         
         /** every time a deposit is made this balance is increased
          *  every time a deposit is required this balance is decreased
          *
          *  This balance cannot be negative without an error.
          */
         std::unordered_map<balance_id_type, asset>       required_deposits;
         std::unordered_map<balance_id_type, asset>       provided_deposits;

         // track deposits and withdraws by asset type
         std::unordered_map<asset_id_type, asset>         deposits;
         std::unordered_map<asset_id_type, asset>         withdraws;
         
         /**
          *  As operation withdraw funds, input balance grows...
          *  As operations consume funds (deposit) input balance decreases
          *
          *  Any left-over input balance can be seen as fees
          *
          *  @note - this value should always equal the sum of deposits-withdraws 
          *  and is maintained for the purpose of seralization.
          */
         std::unordered_map<asset_id_type, share_type>    balance;


         struct vote_state
         {
            vote_state():votes_for(0),votes_against(0){}
         
            int64_t votes_for;
            int64_t votes_against;
         };
         /**
          *  Tracks the votes for or against each delegate based upon 
          *  the deposits and withdraws to addresses.
          */
         std::unordered_map<name_id_type, vote_state>     net_delegate_votes;
      protected:
         chain_interface_ptr                              _current_state;
   };

   typedef std::shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

   struct transaction_location
   {
      transaction_location( uint32_t block_num = 0, uint32_t trx_num = 0 )
      :block_num(0),trx_num(0){}

      uint32_t block_num;
      uint32_t trx_num;
   };

   typedef fc::optional<transaction_location> otransaction_location;


} } // bts::blockchain 

FC_REFLECT( bts::blockchain::transaction, (expiration)(delegate_id)(operations) )
FC_REFLECT_DERIVED( bts::blockchain::signed_transaction, (bts::blockchain::transaction), (signatures) )
FC_REFLECT( bts::blockchain::transaction_evaluation_state::vote_state, (votes_for)(votes_against) )
FC_REFLECT( bts::blockchain::transaction_evaluation_state, 
           (trx)(signed_keys)(required_keys)
           (validation_error_code)
           (validation_error_data)
           (required_deposits)
           (provided_deposits)
           (deposits)(withdraws)(balance)(net_delegate_votes)(balance) )

FC_REFLECT( bts::blockchain::transaction_location, (block_num)(trx_num) )
FC_REFLECT( bts::blockchain::transaction_summary_details, (account)(category)(address)(amount)(asset_id) )
FC_REFLECT( bts::blockchain::transaction_summary, (amount)(confirmations)(blockhash)(blockindex)(blocktime)(txid)(time)(timereceived)(details)(fees)(amounts)(hex)(json_data) )

