#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>
#include <bts/rpc/rpc_client_api.hpp>
#include <bts/api/common_api.hpp>
#include <bts/rpc_stubs/common_api_client.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/logger_config.hpp>
#include <memory>
#include <boost/program_options.hpp>


namespace bts { namespace rpc {
  class rpc_server;
  typedef std::shared_ptr<rpc_server> rpc_server_ptr;
} }
namespace bts { namespace cli {
    class cli;
}};

namespace bts { namespace client {

    using namespace bts::blockchain;
    using namespace bts::wallet;

    boost::program_options::variables_map parse_option_variables(int argc, char** argv);
    fc::path get_data_dir(const boost::program_options::variables_map& option_variables);

    namespace detail { class client_impl; }

    using namespace bts::rpc;

    struct rpc_server_config
    {
      rpc_server_config()
      : enable(false),
        rpc_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
        httpd_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
        htdocs("./htdocs")
      {}

      bool             enable;
      std::string      rpc_user;
      std::string      rpc_password;
      fc::ip::endpoint rpc_endpoint;
      fc::ip::endpoint httpd_endpoint;
      fc::path         htdocs;

      bool is_valid() const; /* Currently just checks if rpc port is set */
    };

    struct config
    {
       config( ) : 
          default_peers(vector<string>{"107.170.30.182:8763"}), 
          ignore_console(false),
          use_upnp(true),
          maximum_number_of_connections(50)
          {
             logging = fc::logging_config::default_config();
          }

          rpc_server_config   rpc;
          vector<string>      default_peers;
          bool                ignore_console;
          bool                use_upnp;
          optional<fc::path>  genesis_config;
          uint16_t            maximum_number_of_connections;
          fc::logging_config  logging;
    };


    /**
     * @class client
     * @brief integrates the network, wallet, and blockchain
     *
     */
    class client : public bts::rpc_stubs::common_api_client, 
                   public std::enable_shared_from_this<client>
    {
       public:
         client();
         client(bts::net::simulated_network_ptr network_to_connect_to);

         virtual ~client();

         void configure_from_command_line(int argc, char** argv);
         fc::future<void> start();
         void open( const path& data_dir, 

                    optional<fc::path> genesis_file_path = optional<fc::path>());

         void init_cli();
         void set_daemon_mode(bool daemon_mode); 


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
         fc::path                   get_data_dir() const;

         // returns true if the client is connected to the network
         bool                is_connected() const;
         bts::net::node_id_t get_node_id() const;

         void configure( const fc::path& configuration_directory );

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port( uint16_t port_to_listen, bool wait_if_not_available);
         void listen_to_p2p_network();
         void connect_to_peer( const string& remote_endpoint );
         void connect_to_p2p_network();

         fc::ip::endpoint get_p2p_listening_endpoint() const;
#ifndef NDEBUG
         bool handle_message(const bts::net::message&, bool sync_mode);
#endif

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

extern const std::string BTS_MESSAGE_MAGIC;

FC_REFLECT(bts::client::client_notification, (timestamp)(message)(signature) )
FC_REFLECT( bts::client::rpc_server_config, (enable)(rpc_user)(rpc_password)(rpc_endpoint)(httpd_endpoint)(htdocs) )
FC_REFLECT( bts::client::config, (rpc)(default_peers)(ignore_console)(logging) )

