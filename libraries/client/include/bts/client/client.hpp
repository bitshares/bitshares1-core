#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>
#include <bts/rpc/rpc_client.hpp>

namespace bts { namespace client {

    using namespace bts::blockchain;
    using namespace bts::wallet;

    namespace detail { class client_impl; }

    using namespace bts::rpc;
    /**
     * @class client
     * @brief integrates the network, wallet, and blockchain
     *
     */
    class client : public bts::rpc::rpc_client_api
    {
       public:
         client( const chain_database_ptr& chain_db );
         virtual ~client();

         void set_chain( const chain_database_ptr& chain );
         void set_wallet( const wallet_ptr& wall );

         /** verifies and then broadcasts the transaction */
         void broadcast_transaction( const signed_transaction& trx );

         /**
          *  Produces a block every 30 seconds if there is at least
          *  once transaction.
          */
         void run_delegate();

         void add_node( const std::string& ep );

         chain_database_ptr     get_chain()const;
         wallet_ptr             get_wallet()const;
         bts::net::node_ptr     get_node()const;
         signed_transactions    get_pending_transactions()const;

         //-------------------------------------------------- JSON-RPC Method Implementations
         //TODO? help()
         //TODO? fc::variant get_info()
         block_id_type          blockchain_get_blockhash(int32_t block_number) const override;
         uint32_t               blockchain_get_blockcount() const override;
         void                   wallet_open_file(const fc::path wallet_filename, const std::string& password) override;
         void                   wallet_open(const std::string& wallet_name, const std::string& password) override;
         void                   wallet_create(const std::string& wallet_name, const std::string& password) override;
         std::string            wallet_get_name() const override;
         void                   wallet_close() override;
         void                   wallet_export_to_json(const fc::path& path) const override;
         void                   wallet_create_from_json(const fc::path& path, 
                                                        const std::string& name, 
                                                        const std::string& passphrase) override;
         void                   wallet_lock();
         void                   wallet_unlock( const fc::microseconds& timeout, 
                                               const std::string& password) override;
         void                   wallet_change_passphrase( const std::string& new_password ) override;
         extended_address       wallet_create_receive_account(const std::string& account_name) override;
         void                   wallet_create_sending_account(const std::string& account_name, 
                                                             const extended_address& account_pub_key) override;
         invoice_summary        wallet_transfer( int64_t amount,
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
         std::map<std::string, extended_address> wallet_list_receive_accounts(uint32_t start = 0, uint32_t count = -1) const;// TODO dan n? this was broke   override;

                  wallet_account_record wallet_get_account(const std::string& account_name) const override;
                 balances               wallet_get_balance( const std::string& asset_symbol = BTS_ADDRESS_PREFIX, 
                                                            const std::string& account_name = "*" ) const override;
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

         bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id) override;
         bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id) override;

         fc::path                            get_data_dir() const;

         // returns true if the client is connected to the network (either server or p2p)
         bool is_connected() const;
         fc::uint160_t get_node_id() const;

         void configure( const fc::path& configuration_directory );

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port( uint16_t port_to_listen );
         void connect_to_peer( const std::string& remote_endpoint );
         void connect_to_p2p_network();
       private:
         std::unique_ptr<detail::client_impl> my;
    };

    typedef std::shared_ptr<client> client_ptr;

    /* Message broadcast on the network to notify all clients of some important information
      (security vulnerability, new version, that sort of thing) */
    class client_notification
    {
    public:
      fc::time_point_sec         timestamp;
      std::string                message;
      fc::ecc::compact_signature signature;

      //client_notification();
      fc::sha256 digest() const;
      void sign(const fc::ecc::private_key& key);
      fc::ecc::public_key signee() const;
    };
    typedef std::shared_ptr<client_notification> client_notification_ptr;

} } // bts::client

FC_REFLECT(bts::client::client_notification, (timestamp)(message)(signature) )
