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


         //TODO? help()
         //TODO? fc::variant get_info()
         virtual bts::blockchain::block_id_type blockchain_get_blockhash(int32_t block_number) const = 0;
         virtual                       uint32_t blockchain_get_blockcount() const = 0;

         virtual                  void wallet_open_file(const fc::path wallet_filename, const std::string& password) = 0;
         virtual                  void wallet_open(const std::string& wallet_name, const std::string& password) = 0;
         virtual                  void wallet_create(const std::string& wallet_name, const std::string& password) = 0;
         virtual           std::string wallet_get_name() const = 0;
         virtual                  void wallet_close() = 0;
         virtual                  void wallet_export_to_json(const fc::path& path) const = 0;
         virtual                  void wallet_create_from_json(const fc::path& path, const std::string& name, const std::string& passphrase) = 0;
         virtual                  void wallet_lock() = 0;
         virtual                  void wallet_unlock(const fc::microseconds& timeout, const std::string& password) = 0;
         virtual                  void wallet_change_passphrase(const std::string& new_password) = 0;
         virtual      extended_address wallet_create_receive_account(const std::string& account_name) = 0;
         virtual                  void wallet_create_sending_account(const std::string& account_name, const extended_address& account_pub_key) = 0;
         virtual       invoice_summary wallet_transfer( int64_t amount,
                                                        const std::string& to_account_name,
                                                         const std::string& asset_symbol = BTS_ADDRESS_PREFIX,
                                                         const std::string& from_account_name = std::string("*"),
                                                         const std::string& invoice_memo = std::string()) = 0;
        virtual             signed_transaction wallet_asset_create(const std::string& symbol,
                                                                   const std::string& asset_name,
                                                                   const std::string& description,
                                                                   const fc::variant& data,
                                                                   const std::string& issuer_name,
                                                                   share_type maximum_share_supply) = 0;
        virtual             signed_transaction wallet_asset_issue(share_type amount,
                                                                  const std::string& symbol,
                                                                  const std::string& to_account_name) = 0;
        virtual std::map<std::string, extended_address> wallet_list_sending_accounts(uint32_t start, uint32_t count) const = 0;
        virtual                std::vector<name_record> wallet_list_reserved_names(const std::string& account_name) const = 0;
        virtual                                    void wallet_rename_account(const std::string& current_account_name, const std::string& new_account_name) = 0;
        virtual std::map<std::string, extended_address> wallet_list_receive_accounts(uint32_t start = 0, uint32_t count = -1) const = 0;

        virtual          wallet_account_record wallet_get_account(const std::string& account_name) const = 0;
        virtual                       balances wallet_get_balance(const std::string& asset_symbol = std::string(BTS_ADDRESS_PREFIX), const std::string& account_name = std::string("*")) const = 0;
        virtual std::vector<wallet_transaction_record> wallet_get_transaction_history(unsigned count) const = 0;
        virtual std::vector<pretty_transaction> wallet_get_transaction_history_summary(unsigned count) const = 0;
        virtual                   oname_record blockchain_get_name_record(const std::string& name) const = 0;
        virtual                   oname_record blockchain_get_name_record_by_id(name_id_type name_id) const = 0;
        virtual                  oasset_record blockchain_get_asset_record(const std::string& symbol) const = 0;
        virtual                  oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const = 0;


         /**
          *  Reserve a name and broadcast it to the network.
          */
        virtual transaction_id_type wallet_reserve_name( const std::string& name, const fc::variant& json_data ) = 0;
        virtual transaction_id_type wallet_update_name( const std::string& name,
                                                        const fc::variant& json_data) = 0; //TODO
        virtual transaction_id_type wallet_register_delegate(const std::string& name, const fc::variant& json_data) = 0;

         /**
         *  Submit and vote on proposals by broadcasting to the network.
         */
         virtual transaction_id_type wallet_submit_proposal(const std::string& name,
                                                     const std::string& subject,
                                                     const std::string& body,
                                                     const std::string& proposal_type,
                                                     const fc::variant& json_data) = 0;
         virtual transaction_id_type wallet_vote_proposal( const std::string& name,
                                                   proposal_id_type proposal_id,
                                                   uint8_t vote) = 0;

         virtual void                               wallet_set_delegate_trust_status(const std::string& delegate_name, int32_t user_trust_level) = 0;
         virtual bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const std::string& delegate_name) const = 0;
         virtual std::map<std::string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const = 0;

         virtual               osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const = 0;
         virtual                        full_block blockchain_get_block(const block_id_type& block_id) const = 0;
         virtual                        full_block blockchain_get_block_by_number(uint32_t block_number) const = 0;

         virtual              void wallet_rescan_blockchain(uint32_t starting_block_number = 0) = 0;
         virtual              void wallet_rescan_blockchain_state() = 0;
         virtual              void wallet_import_bitcoin(const fc::path& filename,const std::string& passphrase) = 0;
         virtual              void wallet_import_private_key(const std::string& wif_key_to_import, 
                                                              const std::string& account_name,
                                                              bool wallet_rescan_blockchain = false) = 0;

     virtual std::vector<name_record> blockchain_get_names(const std::string& first, uint32_t count) const = 0;
     virtual std::vector<asset_record> blockchain_get_assets(const std::string& first_symbol, uint32_t count) const = 0;
     virtual std::vector<name_record> blockchain_get_delegates(uint32_t first, uint32_t count) const = 0;

          virtual               void stop() = 0;

          virtual        uint32_t network_get_connection_count() const = 0;
          virtual    fc::variants network_get_peer_info() const = 0;
          virtual            void network_set_advanced_node_parameters(const fc::variant_object& params) = 0;
          virtual            void network_add_node(const fc::ip::endpoint& node, const std::string& command) = 0;

         virtual bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id) = 0;
         virtual bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id) = 0;

    };

  /**
  *  @class rpc_client
  *  @brief provides a C++ interface to a remote BTS client over JSON-RPC
  */
  class rpc_client : public rpc_client_api
  {
  public:
    rpc_client();
    virtual ~rpc_client();

    void connect_to(const fc::ip::endpoint& remote_endpoint);

    bool login(const std::string& username, const std::string& password);


         //-------------------------------------------------- JSON-RPC Method Implementations
         //TODO? help()
         //TODO? fc::variant get_info()
         fc::variant_object get_info();
         bts::blockchain::block_id_type blockchain_get_blockhash(int32_t block_number) const override;
                               uint32_t blockchain_get_blockcount() const override;
                                   void wallet_open_file(const fc::path wallet_filename, const std::string& password) override;
                                   void wallet_open(const std::string& wallet_name, const std::string& password) override;
                                   void wallet_create(const std::string& wallet_name, const std::string& password) override;
                            std::string wallet_get_name() const override;
                                   void wallet_close() override;
                                   void wallet_export_to_json(const fc::path& path) const override;
                                   void wallet_create_from_json(const fc::path& path, const std::string& name, const std::string& passphrase) override;
                                   void wallet_lock() override;
                                   void wallet_unlock(const fc::microseconds& timeout, const std::string& password) override;
                                   void wallet_change_passphrase(const std::string& new_password) override;
                       extended_address wallet_create_receive_account(const std::string& account_name) override;
                                   void wallet_create_sending_account(const std::string& account_name, const extended_address& account_pub_key) override;
                        invoice_summary wallet_transfer( int64_t amount,
                                                         const std::string& to_account_name,
                                                         const std::string& asset_symbol = BTS_ADDRESS_PREFIX,
                                                         const std::string& from_account_name = std::string("*"),
                                                         const std::string& invoice_memo = std::string()) override;
                     signed_transaction wallet_asset_create(const std::string& symbol,
                                                            const std::string& asset_name,
                                                            const std::string& description,
                                                            const fc::variant& data,
                                                            const std::string& issuer_name,
                                                            share_type maximum_share_supply) override;
                     signed_transaction wallet_asset_issue(share_type amount,
                                                          const std::string& symbol,
                                                          const std::string& to_account_name) override;
         std::map<std::string, extended_address> wallet_list_sending_accounts(uint32_t start, uint32_t count) const override;
                        std::vector<name_record> wallet_list_reserved_names(const std::string& account_name) const override;
                                            void wallet_rename_account(const std::string& current_account_name, const std::string& new_account_name) override;
         std::map<std::string, extended_address> wallet_list_receive_accounts(uint32_t start = 0, uint32_t count = -1) const override;

                  wallet_account_record wallet_get_account(const std::string& account_name) const override;
                                balances wallet_get_balance(const std::string& symbol = BTS_ADDRESS_PREFIX, const std::string& account_name = "*") const override;

         std::vector<wallet_transaction_record> wallet_get_transaction_history(unsigned count) const override;
         std::vector<pretty_transaction> wallet_get_transaction_history_summary(unsigned count) const override;
                           oname_record blockchain_get_name_record(const std::string& name) const override;
                           oname_record blockchain_get_name_record_by_id(name_id_type name_id) const override;
                          oasset_record blockchain_get_asset_record(const std::string& symbol) const override;
                          oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const override;


         /**
          *  Reserve a name and broadcast it to the network.
          */
         transaction_id_type wallet_reserve_name( const std::string& name, const fc::variant& json_data ) override;
         transaction_id_type wallet_update_name( const std::string& name,
                                                 const fc::variant& json_data) override; //TODO
         transaction_id_type wallet_register_delegate(const std::string& name, const fc::variant& json_data) override;

         /**
         *  Submit and vote on proposals by broadcasting to the network.
         */
         transaction_id_type wallet_submit_proposal(const std::string& name,
                                                     const std::string& subject,
                                                     const std::string& body,
                                                     const std::string& proposal_type,
                                                     const fc::variant& json_data) override;
         transaction_id_type wallet_vote_proposal( const std::string& name,
                                                   proposal_id_type proposal_id,
                                                   uint8_t vote) override;

         void                               wallet_set_delegate_trust_status(const std::string& delegate_name, int32_t user_trust_level) override;
         bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const std::string& delegate_name) const override;
         std::map<std::string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const override;

                        osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const override;
                                 full_block blockchain_get_block(const block_id_type& block_id) const override;
                                 full_block blockchain_get_block_by_number(uint32_t block_number) const override;

                       void wallet_rescan_blockchain(uint32_t starting_block_number = 0) override;
                       void wallet_rescan_blockchain_state() override;
                       void wallet_import_bitcoin(const fc::path& filename,const std::string& passphrase) override;
                       void wallet_import_private_key(const std::string& wif_key_to_import, 
                                                      const std::string& account_name,
                                                      bool wallet_rescan_blockchain = false) override;

     std::vector<name_record> blockchain_get_names(const std::string& first, uint32_t count) const override;
    std::vector<asset_record> blockchain_get_assets(const std::string& first_symbol, uint32_t count) const override;
     std::vector<name_record> blockchain_get_delegates(uint32_t first, uint32_t count) const override;

                         void stop() override;

                  uint32_t network_get_connection_count() const override;
              fc::variants network_get_peer_info() const override;
                      void network_set_advanced_node_parameters(const fc::variant_object& params) override;
                      void network_add_node(const fc::ip::endpoint& node, const std::string& command) override;
                      void network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers);


         bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id) override;
         bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id) override;

  private:
    std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
