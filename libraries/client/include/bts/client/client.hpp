#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>

namespace bts { namespace client {

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

         bts::blockchain::chain_database_ptr get_chain()const;
         bts::wallet::wallet_ptr             get_wallet()const;
    
       private:
         std::unique_ptr<detail::client_impl> my;
    };

} } // bts::client
