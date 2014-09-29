#pragma once

#include <bts/blockchain/delegate_slate.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/proposal_record.hpp>
#include <bts/blockchain/withdraw_types.hpp>

#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {

   class chain_interface;
   typedef std::shared_ptr<chain_interface> chain_interface_ptr;
   typedef std::weak_ptr<chain_interface> chain_interface_weak_ptr;

   struct market_index_key;

   /**
    *  A transaction is a set of operations that are
    *  performed atomicly and must be internally consistant.
    *
    *  Every transaction votes for
    */
   struct transaction
   {
      transaction(){}

      digest_type                 digest( const digest_type& chain_id )const;

      fc::time_point_sec          expiration;
      /**
       *  Some transactions such as bids/asks/options require a payout
       *  as a condition of claiming the funds.  Ie: to claim a bid, you
       *  must pay the bidder the proper amount.  When making this payout
       *  the system needs to know which delegate_id to use.
       */
      optional<slate_id_type>     delegate_slate_id; // delegate being voted for in required payouts
      vector<operation>           operations;

      void issue( const asset& amount_to_issue );

      void define_delegate_slate( delegate_slate s );

      void withdraw( const balance_id_type& account,
                     share_type amount );

      void withdraw_pay( const account_id_type& account,
                         share_type amount );

      void deposit( const address& addr,
                    const asset& amount,
                    slate_id_type delegate_id );

      void deposit_to_account( fc::ecc::public_key receiver_key,
                                asset amount,
                                fc::ecc::private_key from_key,
                                const string& memo_message,
                                slate_id_type delegate_id,
                                const fc::ecc::public_key& memo_public_key,
                                fc::ecc::private_key one_time_private_key,
                                memo_flags_enum memo_type = from_memo );


      void register_account( const string& name,
                             const variant& public_data,
                             const public_key_type& master,
                             const public_key_type& active,
                             share_type pay_rate );

      void update_account( account_id_type account_id,
                        share_type delegate_pay_rate,
                        const optional<variant>& public_data,
                        const optional<public_key_type>& active );

      void submit_proposal( account_id_type delegate_id,
                            const string& subject,
                            const string& body,
                            const string& proposal_type,
                            const variant& public_data);

      void vote_proposal(proposal_id_type proposal_id,
                         account_id_type voter_id,
                         proposal_vote::vote_type vote,
                         const string& message );


      void create_asset( const string& symbol,
                         const string& name,
                         const string& description,
                         const variant& data,
                         account_id_type issuer_id,
                         share_type   max_share_supply,
                         int64_t      precision );

      void burn( const asset& quantity,
                 account_id_type for_or_against,
                 const string& public_message,
                 const fc::optional<signature_type>& message_sig );

      void bid( const asset& quantity,
                const price& price_per_unit,
                const address& owner );

      void ask( const asset& quantity,
                const price& price_per_unit,
                const address& owner );

      void short_sell( const asset& quantity,
                const price& collateal_per_usd,
                const address& owner,
                const optional<price>& limit_price = optional<price>() );

      void cover( const asset& quantity,
                  const market_index_key& order_idx );

      void add_collateral( share_type collateral_amount,
                           const market_index_key& order_idx );

      void publish_feed( feed_id_type feed_id,
                         account_id_type delegate_id,
                         fc::variant value );

      bool is_cancel()const;
   }; // transaction

   struct signed_transaction : public transaction
   {
      transaction_id_type                     id()const;
      transaction_id_type                     permanent_id()const;
      size_t                                  data_size()const;
      void                                    sign( const fc::ecc::private_key& signer, const digest_type& chain_id );

      vector<fc::ecc::compact_signature> signatures;
   };
   typedef vector<signed_transaction> signed_transactions;
   typedef optional<signed_transaction> osigned_transaction;

   struct transaction_location
   {
      transaction_location( uint32_t block_num = 0, uint32_t trx_num = 0 )
      :block_num(block_num),trx_num(trx_num){}

      uint32_t block_num;
      uint32_t trx_num;
   };
   typedef optional<transaction_location> otransaction_location;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::transaction, (expiration)(delegate_slate_id)(operations) )
FC_REFLECT_DERIVED( bts::blockchain::signed_transaction, (bts::blockchain::transaction), (signatures) )
FC_REFLECT( bts::blockchain::transaction_location, (block_num)(trx_num) )
