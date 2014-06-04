#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>
#include <bts/rpc/rpc_client_api.hpp>
#include <bts/api/common_api.hpp>
#include <bts/rpc_stubs/common_api_client.hpp>

namespace bts { namespace rpc {
  class rpc_server;
  typedef std::shared_ptr<rpc_server> rpc_server_ptr;
} }

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
                   public bts::rpc_stubs::common_api_client
    {
       public:
                  client();
                  client(bts::net::simulated_network_ptr network_to_connect_to);
         virtual ~client();
         void open( const path& data_dir, const path& genesis_dat );

         /**
          *  Produces a block every 30 seconds if there is at least
          *  once transaction.
          */
         void run_delegate();

         void add_node( const string& ep );

         chain_database_ptr         get_chain()const;
         wallet_ptr                 get_wallet()const;
         bts::rpc::rpc_server_ptr   get_rpc_server() const;
         bts::net::node_ptr         get_node()const;


                       void wallet_rescan_blockchain_state()  override;

                       void wallet_import_bitcoin(const fc::path& filename, 
                                                  const string& passphrase, 
                                                  const string& account_name )  override;


     vector<asset_record> blockchain_get_assets(const string& first_symbol, uint32_t count) const  override;

         fc::path                            get_data_dir() const;

         // returns true if the client is connected to the network
         bool is_connected() const;
         fc::uint160_t get_node_id() const;

         void configure( const fc::path& configuration_directory );

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port( uint16_t port_to_listen );
         void connect_to_peer( const string& remote_endpoint );
         void connect_to_p2p_network();
         fc::ip::endpoint get_p2p_listening_endpoint() const;
       protected:
         virtual bts::api::common_api* get_impl() const override;
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
