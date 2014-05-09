#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>

namespace bts { namespace client {

    using namespace bts::blockchain;

    namespace detail { class client_impl; }

    /**
     * @class client
     * @brief integrates the network, wallet, and blockchain
     *
     */
    class client
    {
       public:
         client(bool enable_p2p = false);
         ~client();

         void set_chain( const bts::blockchain::chain_database_ptr& chain );
         void set_wallet( const bts::wallet::wallet_ptr& wall );

         /** verifies and then broadcasts the transaction */
         void broadcast_transaction( const signed_transaction& trx );

         /**
          *  Produces a block every 30 seconds if there is at least
          *  once transaction.
          */
         void run_trustee( const fc::ecc::private_key& k );

         void add_node( const std::string& ep );

         bts::blockchain::chain_database_ptr get_chain()const;
         bts::wallet::wallet_ptr             get_wallet()const;
         bts::net::node_ptr                  get_node()const;
         signed_transactions                 get_pending_transactions()const;

         /**
          *  Reserve a name and broadcast it to the network.
          */
         transaction_id_type reserve_name( const std::string& name, const fc::variant& data );
         transaction_id_type register_delegate( const std::string& name, const fc::variant& data );

         fc::path                            get_data_dir()const;

         // returns true if the client is connected to the network (either server or p2p)
         bool is_connected() const;
         uint32_t get_connection_count() const;
         fc::variants get_peer_info() const;
         void set_advanced_node_parameters(const fc::variant_object& params);
         void addnode(const fc::ip::endpoint& node, const std::string& command);
         void stop();
         bts::net::message_propagation_data get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id);
         bts::net::message_propagation_data get_block_propagation_data(const bts::blockchain::block_id_type& block_id);
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
