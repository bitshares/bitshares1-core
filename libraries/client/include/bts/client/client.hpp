#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>

namespace bts { namespace client {

    using namespace bts::blockchain;
    using namespace bts::wallet;

    namespace detail { class client_impl; }

    /**
     * @class client
     * @brief integrates the network, wallet, and blockchain
     *
     */
    class client
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
         bts::blockchain::block_id_type blockchain_get_blockhash(int32_t block_number) const;
                               uint32_t blockchain_get_blockcount() const;
                                   void wallet_open_file(const fc::path wallet_filename, const std::string& password);
                                   void wallet_open(const std::string& wallet_name, const std::string& password);
                                   void wallet_create(const std::string& wallet_name, const std::string& password);
                            std::string wallet_get_name() const;
                                   void wallet_close();
                                   void wallet_export_to_json(const fc::path& path) const;
                                   void wallet_create_from_json(const fc::path& path, const std::string& name, const std::string& passphrase);
                                   void wallet_lock();
                                   void wallet_unlock(const fc::microseconds& timeout, const std::string& password);
                                   void wallet_change_passphrase(const std::string& new_password);
                  wallet_account_record wallet_create_receive_account(const std::string& account_name);
                                   void wallet_create_sending_account(const std::string& account_name, const extended_public_key& account_pub_key);
                        invoice_summary wallet_transfer( int64_t amount,
                                                         const std::string& to_account_name,
                                                         const std::string asset_symbol,
                                                         const std::string& from_account_name,
                                                         const std::string& invoice_memo);
                     signed_transaction wallet_asset_create(const std::string& symbol,
                                                            const std::string& asset_name,
                                                            const std::string& description,
                                                            const fc::variant& data,
                                                            const std::string& issuer_name,
                                                            share_type maximum_share_supply);
                     signed_transaction wallet_asset_issue(share_type amount,
                                                          const std::string& symbol,
                                                          const std::string& to_account_name);



         /**
          *  Reserve a name and broadcast it to the network.
          */
         transaction_id_type reserve_name( const std::string& name, const fc::variant& json_data );
         transaction_id_type update_name( const std::string& name,
                                          const fc::optional<fc::variant>& json_data); //TODO
         transaction_id_type register_delegate(const std::string& name, const fc::variant& json_data);

         /**
         *  Submit and vote on proposals by broadcasting to the network.
         */
         transaction_id_type submit_proposal(const std::string& name,
                                             const std::string& subject,
                                             const std::string& body,
                                             const std::string& proposal_type,
                                             const fc::variant& json_data);
         transaction_id_type vote_proposal( const std::string& name,
                                             proposal_id_type proposal_id,
                                             uint8_t vote);

         void                               set_delegate_trust_status(const std::string& delegate_name, int32_t user_trust_level);
         bts::wallet::delegate_trust_status get_delegate_trust_status(const std::string& delegate_name) const;
         std::map<std::string, bts::wallet::delegate_trust_status> list_delegate_trust_status() const;



         fc::path                            get_data_dir()const;

         // returns true if the client is connected to the network (either server or p2p)
         bool is_connected() const;
         uint32_t get_connection_count() const;
         fc::variants get_peer_info() const;
         void set_advanced_node_parameters(const fc::variant_object& params);
         void addnode(const fc::ip::endpoint& node, const std::string& command);
         void stop();
         bts::net::message_propagation_data get_transaction_propagation_data(const transaction_id_type& transaction_id);
         bts::net::message_propagation_data get_block_propagation_data(const block_id_type& block_id);
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
