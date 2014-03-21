#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/chain_client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace client {

    namespace detail 
    { 
       class client_impl : public bts::net::chain_client_delegate
       {
          public:

            virtual void on_new_block( const trx_block& block )
            {
               _wallet->scan_chain( *_chain_db, block.block_num );
            }
            virtual void on_new_transaction( const signed_transaction& trx )
            {
            }

            client_impl()
            {
               _chain_client.set_delegate( this );
            }

            bts::net::chain_client               _chain_client;
            bts::blockchain::chain_database_ptr  _chain_db;
            bts::wallet::wallet_ptr              _wallet;
       };
    }

    client::client()
    :my( new detail::client_impl() )
    {
    }

    client::~client(){}

    void client::set_chain( const bts::blockchain::chain_database_ptr& ptr )
    {
       my->_chain_db = ptr;
       my->_chain_client.set_chain( ptr );
    }

    void client::set_wallet( const bts::wallet::wallet_ptr& wall )
    {
       my->_wallet = wall;
    }

    bts::wallet::wallet_ptr client::get_wallet()const { return my->_wallet; }

    void client::broadcast_transaction( const signed_transaction& trx )
    {
       my->_chain_client.broadcast_transaction( trx );
    }

    void client::add_node( const std::string& ep )
    {
       my->_chain_client.add_node(ep);
    }


} } // bts::client
