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

         // returns true if the client is connected to the network (either server or p2p)
         bool is_connected() const;

         // functions for taking command-line parameters and passing them on to the p2p node
         void listen_on_port(uint16_t port_to_listen);
         void load_p2p_configuration(const fc::path& configuration_directory);
         void connect_to_peer(const std::string& remote_endpoint);
         void connect_to_p2p_network();
       private:
         std::unique_ptr<detail::client_impl> my;
    };

    typedef std::shared_ptr<client> client_ptr;

} } // bts::client
