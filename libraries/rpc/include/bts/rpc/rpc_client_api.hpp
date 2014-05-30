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

    typedef std::vector<std::pair<share_type,std::string> > balances;


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
                                                        const std::string& to_account_name,
                                                         const std::string& asset_symbol = BTS_ADDRESS_PREFIX,
                                                         const std::string& from_account_name = std::string("*"),
                                                         const std::string& invoice_memo = std::string(),
                                                         generate_transaction_flag flag = sign_and_broadcast) = 0;
                                                         */
        virtual             signed_transaction wallet_asset_create(const std::string& symbol,
                                                                   const std::string& asset_name,
                                                                   const std::string& description,
                                                                   const fc::variant& data,
                                                                   const std::string& issuer_name,
                                                                   share_type maximum_share_supply,
                                                                   generate_transaction_flag flag = sign_and_broadcast) = 0;


        virtual             signed_transaction wallet_asset_issue(share_type amount,
                                                                  const std::string& symbol,
                                                                  const std::string& to_account_name,
                                                                  generate_transaction_flag flag = sign_and_broadcast) = 0;


         /**
         *  Submit and vote on proposals
         */
         virtual signed_transaction wallet_submit_proposal(const std::string& name,
                                                     const std::string& subject,
                                                     const std::string& body,
                                                     const std::string& proposal_type,
                                                     const fc::variant& json_data,
                                                     generate_transaction_flag = sign_and_broadcast) = 0;
         virtual signed_transaction wallet_vote_proposal( const std::string& name,
                                                           proposal_id_type proposal_id,
                                                           uint8_t vote,
                                                           generate_transaction_flag = sign_and_broadcast) = 0;


        virtual                std::vector<name_record> wallet_list_reserved_names(const std::string& account_name) const = 0;
        virtual                                    void wallet_rename_account(const std::string& current_account_name, const std::string& new_account_name) = 0;
        virtual std::map<std::string, public_key_type> wallet_list_receive_accounts() const = 0;

        virtual          wallet_account_record wallet_get_account(const std::string& account_name) const = 0;
        virtual                       balances wallet_get_balance(const std::string& asset_symbol = std::string(BTS_ADDRESS_PREFIX), const std::string& account_name = std::string("*")) const = 0;
        virtual std::vector<wallet_transaction_record> wallet_get_transaction_history(unsigned count) const = 0;
        virtual std::vector<pretty_transaction> wallet_get_transaction_history_summary(unsigned count) const = 0;
        virtual                   oname_record blockchain_get_account_record(const std::string& name) const = 0;
        virtual                   oname_record blockchain_get_account_record_by_id(name_id_type name_id) const = 0;
        virtual                  oasset_record blockchain_get_asset_record(const std::string& symbol) const = 0;
        virtual                  oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const = 0;


         virtual void                               wallet_set_delegate_trust_status(const std::string& delegate_name, int32_t user_trust_level) = 0;
         //virtual bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const std::string& delegate_name) const = 0;
         //virtual std::map<std::string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const = 0;

         virtual               osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const = 0;
         virtual                        full_block blockchain_get_block(const block_id_type& block_id) const = 0;
         virtual                        full_block blockchain_get_block_by_number(uint32_t block_number) const = 0;

         virtual              void wallet_rescan_blockchain(uint32_t starting_block_number = 0) = 0;
         virtual              void wallet_rescan_blockchain_state() = 0;
         virtual              void wallet_import_bitcoin(const fc::path& filename,const std::string& passphrase, const std::string& account_name ) = 0;
         virtual              void wallet_import_private_key(const std::string& wif_key_to_import, 
                                                              const std::string& account_name,
                                                              bool wallet_rescan_blockchain = false) = 0;

         virtual std::vector<name_record> blockchain_get_names(const std::string& first, uint32_t count) const = 0;
         virtual std::vector<asset_record> blockchain_get_assets(const std::string& first_symbol, uint32_t count) const = 0;
         virtual std::vector<name_record> blockchain_get_delegates(uint32_t first, uint32_t count) const = 0;

          virtual               void stop() = 0;

          virtual            void network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers) = 0;
          
         virtual bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id) = 0;
         virtual bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id) = 0;
    };


} } // bts::rpc
