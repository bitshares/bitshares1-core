#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/object_record.hpp>
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

      void issue( const asset& amount_to_issue );

      void set_object( const object_record& obj );
      void set_edge( const edge_record& edge );

      void define_slate( const set<account_id_type>& slate );

      void withdraw( const balance_id_type& account, share_type amount );

      void withdraw_pay( const account_id_type account, share_type amount );

      void deposit( const address& addr, const asset& amount );

      void authorize_key( asset_id_type asset_id, const address& owner, object_id_type meta );

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

      public_key_type deposit_to_account(fc::ecc::public_key receiver_key,
                                         asset amount,
                                         fc::ecc::private_key from_key,
                                         const string& memo_message,
                                         const fc::ecc::public_key& memo_public_key,
                                         fc::ecc::private_key one_time_private_key,
                                         memo_flags_enum memo_type = from_memo,
                                         bool use_stealth_address = true);


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
                         const variant& data,
                         account_id_type issuer_id,
                         share_type max_share_supply,
                         uint64_t precision );

      void update_asset( const asset_id_type asset_id,
                         const optional<string>& name,
                         const optional<string>& description,
                         const optional<variant>& public_data,
                         const optional<double>& maximum_share_supply,
                         const optional<uint64_t>& precision );

      void update_asset_ext( const asset_id_type asset_id,
                         const optional<string>& name,
                         const optional<string>& description,
                         const optional<variant>& public_data,
                         const optional<double>& maximum_share_supply,
                         const optional<uint64_t>& precision,
                         const share_type issuer_fee,
                         uint16_t market_fee,
                         uint32_t flags,
                         uint32_t issuer_permissions,
                         account_id_type issuer_account_id,
                         uint32_t required_sigs,
                         const vector<address>& authority
                         );

      void burn( const asset& quantity,
                 account_id_type for_or_against,
                 const string& public_message,
                 const fc::optional<signature_type>& message_sig );

      void bid( const asset& quantity,
                const price& price_per_unit,
                const address& owner );

      void relative_bid( const asset& quantity,
                const price& price_per_unit,
                const optional<price>& limit,
                const address& owner );

      void ask( const asset& quantity,
                const price& price_per_unit,
                const address& owner );

      void relative_ask( const asset& quantity,
                const price& price_per_unit,
                const optional<price>& limit,
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
