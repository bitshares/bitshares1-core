#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>
#include <bts/rpc/rpc_client_api.hpp>
#include <bts/api/common_api.hpp>

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
    class client : public bts::rpc::rpc_client_api,
                   public bts::api::common_api
    {
       public:
         enum generate_transaction_flag
         {
            sign_and_broadcast    = 0,
            do_not_broadcast      = 1,
            do_not_sign           = 2
         };

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

         void add_node( const string& ep );

         chain_database_ptr     get_chain()const;
         wallet_ptr             get_wallet()const;
         bts::net::node_ptr     get_node()const;
         signed_transactions    get_pending_transactions()const;

         //-------------------------------------------------- JSON-RPC Method Implementations
#include <bts/rpc_stubs/common_api_overrides.ipp>

         signed_transaction  wallet_asset_create( const string& symbol,
                                                  const string& asset_name,
                                                  const string& description,
                                                  const fc::variant& data,
                                                  const string& issuer_name,
                                                  share_type maximum_share_supply,
                                                  rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast)  override;

         signed_transaction  wallet_asset_issue( share_type amount,
                                                 const string& symbol,
                                                 const string& to_account_name,
                                                 rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast)  override;
         /**
          *  Reserve a name and broadcast it to the network.
          */
         signed_transaction  wallet_register_account( const string& account_name, 
                                                      const string& pay_with_account,
                                                      const fc::variant& json_data = fc::variant(),
                                                      bool as_delegate = false,
                                                      rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast );

         signed_transaction wallet_update_registered_account( const string& registered_account_name,
                                                              const fc::variant& json_data = fc::variant(),
                                                              bool as_delegate = false,
                                                              rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast); 



         /**
         *  Submit and vote on proposals by broadcasting to the network.
         */
         signed_transaction wallet_submit_proposal(const string& name,
                                                     const string& subject,
                                                     const string& body,
                                                     const string& proposal_type,
                                                     const fc::variant& json_data,
                                                     rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast)  override;
         signed_transaction wallet_vote_proposal( const string& name,
                                                   proposal_id_type proposal_id,
                                                   uint8_t vote,
                                                   rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast)  override;


         map<string, public_key_type> wallet_list_contact_accounts() const;
         map<string, public_key_type> wallet_list_receive_accounts() const override;

                        vector<name_record> wallet_list_reserved_names(const string& account_name) const  override;
                                            void wallet_rename_account(const string& current_account_name, const string& new_account_name)  override;

                  wallet_account_record wallet_get_account(const string& account_name) const  override;
                 balances               wallet_get_balance( const string& asset_symbol = BTS_ADDRESS_PREFIX, 
                                                            const string& account_name = "*" ) const  override;
         vector<wallet_transaction_record> wallet_get_transaction_history(unsigned count) const  override;
         vector<pretty_transaction> wallet_get_transaction_history_summary(unsigned count) const  override;
                           oname_record blockchain_get_name_record(const string& name) const  override;
                           oname_record blockchain_get_name_record_by_id(name_id_type name_id) const  override;
                          oasset_record blockchain_get_asset_record(const string& symbol) const  override;
                          oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const  override;


         void                               wallet_set_delegate_trust_status(const string& delegate_name, int32_t user_trust_level)  override;
         //bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const string& delegate_name) const  override;
        // map<string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const  override;

                        osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const  override;
                                 full_block blockchain_get_block(const block_id_type& block_id) const  override;
                                 full_block blockchain_get_block_by_number(uint32_t block_number) const  override;

                       void wallet_rescan_blockchain(uint32_t starting_block_number = 0)  override;
                       void wallet_rescan_blockchain_state()  override;

                       void wallet_import_bitcoin(const fc::path& filename, 
                                                  const string& passphrase, 
                                                  const string& account_name )  override;

                       void wallet_import_private_key(const string& wif_key_to_import, 
                                                      const string& account_name,
                                                      bool wallet_rescan_blockchain = false)  override;

     vector<name_record> blockchain_get_names(const string& first, uint32_t count) const  override;
    vector<asset_record> blockchain_get_assets(const string& first_symbol, uint32_t count) const  override;
     vector<name_record> blockchain_get_delegates(uint32_t first, uint32_t count) const  override;

                         void stop()  override;

                      void network_set_allowed_peers(const vector<bts::net::node_id_t>& allowed_peers)  override;
                      void network_set_advanced_node_parameters(const fc::variant_object& params)  override;
                      fc::variant_object network_get_advanced_node_parameters()  override;

         bts::net::message_propagation_data network_get_transaction_propagation_data(const transaction_id_type& transaction_id)  override;
         bts::net::message_propagation_data network_get_block_propagation_data(const block_id_type& block_id)  override;

         fc::path                            get_data_dir() const;

         // returns true if the client is connected to the network (either server or p2p)
         bool is_connected() const;
         fc::uint160_t get_node_id() const;

         void configure( const fc::path& configuration_directory );

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port( uint16_t port_to_listen );
         void connect_to_peer( const string& remote_endpoint );
         void connect_to_p2p_network();
       private:
         unique_ptr<detail::client_impl> my;
    };

    typedef shared_ptr<client> client_ptr;

    /* Message broadcast on the network to notify all clients of some important information
      (security vulnerability, new version, that sort of thing) */
    class client_notification
    {
    public:
      fc::time_point_sec         timestamp;
      string                message;
      fc::ecc::compact_signature signature;

      //client_notification();
      fc::sha256 digest() const;
      void sign(const fc::ecc::private_key& key);
      fc::ecc::public_key signee() const;
    };
    typedef shared_ptr<client_notification> client_notification_ptr;

} } // bts::client

FC_REFLECT(bts::client::client_notification, (timestamp)(message)(signature) )
FC_REFLECT_ENUM( bts::client::client::generate_transaction_flag, (do_not_broadcast)(do_not_sign)(sign_and_broadcast) )
