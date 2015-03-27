#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/slate_record.hpp>
#include <bts/blockchain/withdraw_types.hpp>

#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {

   struct market_index_key;

   struct transaction
   {
      fc::time_point_sec    expiration;
      optional<uint64_t>    reserved;
      vector<operation>     operations;

      digest_type digest( const digest_type& chain_id )const;

      void define_slate( const set<account_id_type>& slate );

      void withdraw( const balance_id_type& account, share_type amount );

      void withdraw_pay( const account_id_type account, share_type amount );

      void deposit_multisig( const multisig_meta_info& info, const asset& amount );

      void release_escrow( const address& escrow_account,
                           const address& released_by,
                           share_type amount_to_sender,
                           share_type amount_to_receiver );

      public_key_type deposit_to_escrow( fc::ecc::public_key receiver_key,
                              fc::ecc::public_key escrow_key,
                              digest_type agreement,
                              asset amount,
                              fc::ecc::private_key from_key,
                              const string& memo_message,
                              const fc::ecc::public_key& memo_public_key,
                              fc::ecc::private_key one_time_private_key,
                              memo_flags_enum memo_type = from_memo );

      void deposit_to_address( const asset& amount, const address& recipient_address );

      public_key_type deposit_with_encrypted_memo( const asset& amount,
                                                   const private_key_type& sender_private_key,
                                                   const public_key_type& recipient_public_key,
                                                   const private_key_type& one_time_private_key,
                                                   const string& memo,
                                                   bool stealth_deposit = false );

      void register_account( const string& name,
                             const variant& public_data,
                             const public_key_type& master,
                             const public_key_type& active,
                             uint8_t pay_rate = -1,
                             optional<account_meta_info> info = optional<account_meta_info>() );

      void update_account( account_id_type account_id,
                        uint8_t delegate_pay_rate,
                        const optional<variant>& public_data,
                        const optional<public_key_type>& active );

      void create_asset( const string& symbol,
                         const string& name,
                         const string& description,
                         const account_id_type issuer_id,
                         const share_type max_supply,
                         const uint64_t precision );

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
                const price& interest_rate,
                const address& owner,
                const optional<price>& limit_price = optional<price>() );

      void cover( const asset& quantity,
                  const market_index_key& order_idx );

      void add_collateral( share_type collateral_amount,
                           const market_index_key& order_idx );

      void publish_feed( asset_id_type feed_id,
                         account_id_type delegate_id,
                         fc::variant value );

      void update_signing_key( const account_id_type account_id, const public_key_type& signing_key );

      void update_balance_vote( const balance_id_type& balance_id, const optional<address>& new_restricted_owner );

      void set_slates( const slate_id_type slate_id );

      void limit_fee( const asset& max_fee );

      void uia_issue( const asset& amount );

      void uia_withdraw_fees( const asset& amount );

      void uia_update_authority_flag_permissions( const asset_id_type asset_id, const uint32_t authority_flag_permissions );

      void uia_update_active_flags( const asset_id_type asset_id, const uint32_t active_flags );

      void uia_add_to_whitelist( const asset_id_type asset_id, const account_id_type account_id );

      void uia_remove_from_whitelist( const asset_id_type asset_id, const account_id_type account_id );

      bool is_cancel()const;
   }; // transaction

   struct signed_transaction : public transaction
   {
      transaction_id_type   id()const;
      size_t                data_size()const;
      void                  sign( const fc::ecc::private_key& signer, const digest_type& chain_id );
      public_key_type       get_signing_key( const size_t sig_index, const digest_type& chain_id )const;

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

FC_REFLECT( bts::blockchain::transaction, (expiration)(reserved)(operations) )
FC_REFLECT_DERIVED( bts::blockchain::signed_transaction, (bts::blockchain::transaction), (signatures) )
FC_REFLECT( bts::blockchain::transaction_location, (block_num)(trx_num) )
