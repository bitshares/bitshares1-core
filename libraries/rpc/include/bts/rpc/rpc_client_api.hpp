#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/net/node.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>




namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }

    using namespace bts::blockchain;
    using namespace bts::wallet;

    typedef vector<std::pair<share_type,string> > balances;


    class rpc_client_api
    {
        public:
         enum generate_transaction_flag
         {
            sign_and_broadcast    = 0,
            do_not_broadcast      = 1,
            do_not_sign           = 2
         };

        public:
         //TODO? help()
         //TODO? fc::variant get_info()
         /*
         virtual       invoice_summary wallet_transfer( int64_t amount,
                                                        const string& to_account_name,
                                                         const string& asset_symbol = BTS_ADDRESS_PREFIX,
                                                         const string& from_account_name = string("*"),
                                                         const string& invoice_memo = string(),
                                                         generate_transaction_flag flag = sign_and_broadcast) = 0;
                                                         */
        virtual             signed_transaction wallet_asset_create(const string& symbol,
                                                                   const string& asset_name,
                                                                   const string& description,
                                                                   const fc::variant& data,
                                                                   const string& issuer_name,
                                                                   share_type maximum_share_supply,
                                                                   generate_transaction_flag flag = sign_and_broadcast) = 0;


        virtual             signed_transaction wallet_asset_issue(share_type amount,
                                                                  const string& symbol,
                                                                  const string& to_account_name,
                                                                  generate_transaction_flag flag = sign_and_broadcast) = 0;


         /**
         *  Submit and vote on proposals
         */
         virtual signed_transaction wallet_submit_proposal(const string& name,
                                                     const string& subject,
                                                     const string& body,
                                                     const string& proposal_type,
                                                     const fc::variant& json_data,
                                                     generate_transaction_flag = sign_and_broadcast) = 0;
         virtual signed_transaction wallet_vote_proposal( const string& name,
                                                           proposal_id_type proposal_id,
                                                           uint8_t vote,
                                                           generate_transaction_flag = sign_and_broadcast) = 0;



        virtual          wallet_account_record wallet_get_account(const string& account_name) const = 0;
        virtual                       balances wallet_get_balance(const string& asset_symbol = string(BTS_ADDRESS_PREFIX), const string& account_name = string("*")) const = 0;
        virtual                   blockchain::oaccount_record blockchain_get_account_record(const string& name) const = 0;
        virtual                   blockchain::oaccount_record blockchain_get_account_record_by_id(name_id_type name_id) const = 0;
        virtual                  oasset_record blockchain_get_asset_record(const string& symbol) const = 0;
        virtual                  oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const = 0;


         virtual void                               wallet_set_delegate_trust_status(const string& delegate_name, int32_t user_trust_level) = 0;
         //virtual bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const string& delegate_name) const = 0;
         //virtual map<string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const = 0;

         virtual               osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const = 0;
         virtual                        full_block blockchain_get_block(const block_id_type& block_id) const = 0;
         virtual                        full_block blockchain_get_block_by_number(uint32_t block_number) const = 0;

         virtual              void wallet_rescan_blockchain(uint32_t starting_block_number = 0) = 0;
         virtual              void wallet_rescan_blockchain_state() = 0;
         virtual              void wallet_import_bitcoin(const fc::path& filename,const string& passphrase, const string& account_name ) = 0;
         virtual              void wallet_import_private_key(const string& wif_key_to_import, 
                                                              const string& account_name,
                                                              bool wallet_rescan_blockchain = false) = 0;

         virtual vector<asset_record> blockchain_get_assets(const string& first_symbol, uint32_t count) const = 0;
         virtual vector<account_record> blockchain_get_delegates(uint32_t first, uint32_t count) const = 0;

          virtual               void stop() = 0;

          virtual            void network_set_allowed_peers(const vector<bts::net::node_id_t>& allowed_peers) = 0;
          
         virtual bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id) = 0;
         virtual bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id) = 0;
    };


} } // bts::rpc
