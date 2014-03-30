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
         client();
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

    
       private:
         std::unique_ptr<detail::client_impl> my;
    };

    typedef std::shared_ptr<client> client_ptr;

} } // bts::client
