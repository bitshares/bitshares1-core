#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/error_codes.hpp>
#include <fc/optional.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>

#include <bts/blockchain/proposal_record.hpp>

#include <unordered_set>

namespace bts { namespace blockchain {

   class chain_interface;
   typedef std::shared_ptr<chain_interface> chain_interface_ptr;
   typedef std::weak_ptr<chain_interface> chain_interface_weak_ptr;
   struct fire_delegate_operation;

   /**
    *  A transaction is a set of operations that are
    *  performed atomicly and must be internally consistant.
    *
    *  Every transaction votes for 
    */
   struct transaction
   {
      transaction(){}

      digest_type                      digest( const digest_type& chain_id )const;

      fc::optional<fc::time_point_sec> expiration;
      /**
       *  Some transactions such as bids/asks/options require a payout
       *  as a condition of claiming the funds.  Ie: to claim a bid, you 
       *  must pay the bidder the proper amount.  When making this payout
       *  the system needs to know which delegate_id to use. 
       */
      fc::optional<name_id_type>       delegate_id; // delegate being voted for in required payouts
      std::vector<operation>           operations; 

      void issue( const asset& amount_to_issue );

      void withdraw( const balance_id_type& account, 
                     share_type amount );

      void withdraw_pay( const account_id_type& account, 
                         share_type amount );

      void deposit( const address& addr, 
                    const asset& amount, 
                    name_id_type delegate_id );

      void deposit_to_account( fc::ecc::public_key receiver_key,
                                asset amount,
                                fc::ecc::private_key from_key,
                                const std::string& memo_message,
                                name_id_type delegate_id,
                                const fc::ecc::public_key& memo_public_key,
                                memo_flags_enum memo_type = from_memo );


      void register_account( const std::string& name, 
                         const fc::variant& public_data, 
                         const public_key_type& master, 
                         const public_key_type& active, 
                         bool as_delegate = false );

      void update_account( name_id_type name_id, 
                        const fc::optional<fc::variant>& public_data, 
                        const fc::optional<public_key_type>& active, 
                        bool as_delegate = false );

      void submit_proposal( name_id_type delegate_id,
                            const std::string& subject,
                            const std::string& body,
                            const std::string& proposal_type,
                            const fc::variant& public_data);

      void vote_proposal(proposal_id_type proposal_id, 
                         name_id_type voter_id, 
                         proposal_vote::vote_type vote,
                         const string& message );


      void create_asset( const std::string& symbol, 
                         const std::string& name, 
                         const std::string& description,
                         const fc::variant& data,
                         name_id_type issuer_id,
                         share_type   max_share_supply,
                         int64_t      precision );
   }; // transaction

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
      fc::variant                                public_data;
   }; // transaction_summary


   struct signed_transaction : public transaction
   {
      transaction_id_type                     id()const;
      size_t                                  data_size()const;
      void                                    sign( const fc::ecc::private_key& signer, const digest_type& chain_id );

      std::vector<fc::ecc::compact_signature> signatures;
   };
   typedef std::vector<signed_transaction> signed_transactions;
   typedef fc::optional<signed_transaction> osigned_transaction;


   struct transaction_location
   {
      transaction_location( uint32_t block_num = 0, uint32_t trx_num = 0 )
      :block_num(block_num),trx_num(trx_num){}

      uint32_t block_num;
      uint32_t trx_num;
   };

   typedef fc::optional<transaction_location> otransaction_location;


} } // bts::blockchain 

FC_REFLECT( bts::blockchain::transaction, (expiration)(delegate_id)(operations) )
FC_REFLECT_DERIVED( bts::blockchain::signed_transaction, (bts::blockchain::transaction), (signatures) )

FC_REFLECT( bts::blockchain::transaction_location, (block_num)(trx_num) )
FC_REFLECT( bts::blockchain::transaction_summary_details, (account)(category)(address)(amount)(asset_id) )
FC_REFLECT( bts::blockchain::transaction_summary, (amount)(confirmations)(blockhash)(blockindex)(blocktime)(txid)(time)(timereceived)(details)(fees)(amounts)(hex)(public_data) )
