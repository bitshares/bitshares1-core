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
    using namespace bts::mail;

    boost::program_options::variables_map parse_option_variables(int argc, char** argv);
    fc::path get_data_dir(const boost::program_options::variables_map& option_variables);
    fc::variant_object version_info();

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

    struct chain_server_config
    {
        chain_server_config()
         : enabled(false),
           listen_port(0)
        {}

        bool enabled;
        uint16_t listen_port;
    };

    struct config
    {
       config( ) :
          default_peers(vector<string>{
                  "5.101.106.138:1776",
                  "5.101.106.138:1777",
                  "5.101.106.138:1778",
                  "61.129.33.213:1776",
                  "80.240.133.79:1776",
                  "80.240.133.79:1777",
                  "80.240.133.79:1778",
                  "84.238.140.192:42577",
                  "89.187.144.203:8764",
                  "95.85.33.16:8764",
                  "104.131.35.149:1776",
                  "178.62.50.61:1776",
                  "178.62.50.61:1777",
                  "178.62.50.61:1778",
                  "178.62.50.61:1779",
                  "178.62.157.161:1776",
                  "180.153.142.120:1777",
                  "188.226.195.137:60696"
#if !defined(_WIN32) || !defined(_M_AMD64)
				  // we're currently experiencing an intermittent crash on 64-bit windows
				  // when adding nodes with symbolic DNS names.  Disable them until
				  // we get it sorted out
				  ,"auseednode.minebitshares.com:1776",
                  "euseednode.minebitshares.com:1776"
#endif
                  }),
          mail_server_enabled(false),
          wallet_enabled(true),
          ignore_console(false),
          use_upnp(true),
          maximum_number_of_connections(BTS_NET_DEFAULT_MAX_CONNECTIONS) ,
          delegate_server( fc::ip::endpoint::from_string("0.0.0.0:0") ),
          default_delegate_peers( vector<string>({"178.62.50.61:9988"}) )
          {
              logging = fc::logging_config::default_config();
          }

          rpc_server_config   rpc;
          vector<string>      default_peers;
          vector<string>      chain_servers;
          chain_server_config chain_server;
          bool                mail_server_enabled;
          bool                wallet_enabled;
          bool                ignore_console;
          bool                use_upnp;
          optional<fc::path>  genesis_config;
          uint16_t            maximum_number_of_connections;
          fc::logging_config  logging;
          fc::ip::endpoint    delegate_server;
          vector<string>      default_delegate_peers;

          fc::optional<std::string> growl_notify_endpoint;
          fc::optional<std::string> growl_password;
          fc::optional<std::string> growl_bitshares_client_identifier;
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

         void simulate_disconnect( bool state );

         virtual ~client();

         void start_networking(std::function<void()> network_started_callback = std::function<void()>());
         void configure_from_command_line(int argc, char** argv);
         fc::future<void> start();
         void open(const path& data_dir,
                   optional<fc::path> genesis_file_path = optional<fc::path>(),
                   std::function<void(float)> reindex_status_callback = std::function<void(float)>());

         void init_cli();
         void set_daemon_mode(bool daemon_mode);


         chain_database_ptr         get_chain()const;
         wallet_ptr                 get_wallet()const;
         mail_client_ptr            get_mail_client()const;
         mail_server_ptr            get_mail_server()const;
         bts::rpc::rpc_server_ptr   get_rpc_server()const;
         bts::net::node_ptr         get_node()const;
         fc::path                   get_data_dir()const;

         // returns true if the client is connected to the network
         bool                is_connected() const;
         bts::net::node_id_t get_node_id() const;

         const config& configure(const fc::path& configuration_directory);

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port( uint16_t port_to_listen, bool wait_if_not_available);
         void accept_incoming_p2p_connections(bool accept);
         void listen_to_p2p_network();
         static fc::ip::endpoint string_to_endpoint(const std::string& remote_endpoint);
         void add_node( const string& remote_endpoint );
         void connect_to_peer( const string& remote_endpoint );
         void connect_to_p2p_network();

         fc::ip::endpoint get_p2p_listening_endpoint() const;
         bool handle_message(const bts::net::message&, bool sync_mode);
         void sync_status(uint32_t item_type, uint32_t item_count);

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
FC_REFLECT( bts::client::chain_server_config, (enabled)(listen_port) )
FC_REFLECT( bts::client::config,
            (rpc)(default_peers)(chain_servers)(chain_server)(mail_server_enabled)
            (wallet_enabled)(ignore_console)(logging)
            (delegate_server)
            (default_delegate_peers)
            (growl_notify_endpoint)
            (growl_password)
            (growl_bitshares_client_identifier) )

