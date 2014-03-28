#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/chain_client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace client {

    namespace detail 
    { 
       class client_impl : public bts::net::chain_client_delegate
       {
          public:

            virtual void on_new_block( const trx_block& block )
            {
               ilog( "" );
               _wallet->scan_chain( *_chain_db, block.block_num );
            }

            virtual void on_new_transaction( const signed_transaction& trx )
            {
            }

            client_impl()
            {
               _chain_client.set_delegate( this );
            }

            void trustee_loop()
            {
               while( !_trustee_loop_complete.canceled() )
               {
                  auto pending_trxs = _chain_client.get_pending_transactions();
                  if( pending_trxs.size() && (fc::time_point::now() - _last_block) > fc::seconds(30) )
                  {
                     try {
                        auto blk = _wallet->generate_next_block( *_chain_db, pending_trxs );
                        blk.sign( _trustee_key );
                       // _chain_db->push_block( blk );
                        _chain_client.broadcast_block( blk );
                        _last_block = fc::time_point::now();
                     } catch ( const fc::exception& e )
                     {
                        elog( "error producing block?: ${e}", ("e",e.to_detail_string() ) );
                     }
                  }
                  fc::usleep( fc::seconds( 1 ) );
               }
            }
            fc::ecc::private_key                                        _trustee_key;
            fc::time_point                                              _last_block;

            bts::blockchain::trx_block           _next_block;
            bts::net::chain_client               _chain_client;
            bts::blockchain::chain_database_ptr  _chain_db;
            bts::wallet::wallet_ptr              _wallet;
            float                                _effort;
            fc::future<void>                     _trustee_loop_complete;
       };
    }

    client::client()
    :my( new detail::client_impl() )
    {
    }

    client::~client()
    {
       try {
          if( my->_trustee_loop_complete.valid() )
          {
             my->_trustee_loop_complete.cancel();
             ilog( "waiting for trustee loop to complete" );
             my->_trustee_loop_complete.wait();
          } 
       }
       catch ( const fc::canceled_exception& ) {}
       catch ( const fc::exception& e )
       {
          wlog( "${e}", ("e",e.to_detail_string() ) );
       }
    }

    void client::set_chain( const bts::blockchain::chain_database_ptr& ptr )
    {
       my->_chain_db = ptr;
       my->_chain_client.set_chain( ptr );
    }

    void client::set_wallet( const bts::wallet::wallet_ptr& wall )
    {
       FC_ASSERT( my->_chain_db );
       my->_wallet = wall;
       my->_wallet->scan_chain( *my->_chain_db, my->_chain_db->head_block_num() );
    }

    bts::wallet::wallet_ptr client::get_wallet()const { return my->_wallet; }
    bts::blockchain::chain_database_ptr client::get_chain()const { return my->_chain_db; }

    void client::broadcast_transaction( const signed_transaction& trx )
    {
       my->_chain_client.broadcast_transaction( trx );
    }

    void client::add_node( const std::string& ep )
    {
       my->_chain_client.add_node(ep);
    }

    void client::run_trustee( const fc::ecc::private_key& k )
    {
       my->_trustee_key = k;
       my->_trustee_loop_complete = fc::async( [=](){ my->trustee_loop(); } );
    }

} } // bts::client
