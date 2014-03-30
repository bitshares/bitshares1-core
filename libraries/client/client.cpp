#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/chain_client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/block_miner.hpp>
#include <fc/reflect/variant.hpp>

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
               start_mining_next_block();
            }

            virtual void on_new_transaction( const signed_transaction& trx )
            {
               ilog( "" );
               start_mining_next_block();
            }

            void start_mining_next_block()
            {
               ilog( "start mining block" );
               int64_t miner_votes = 0;
               _next_block = _wallet->generate_next_block( *_chain_db, _chain_client.get_pending_transactions(), miner_votes ); 
               auto head_block = _chain_db->get_head_block();
               _miner.set_block( _next_block, head_block, miner_votes, head_block.min_votes() );
            }

            client_impl()
            {
               _chain_client.set_delegate( this );
               _miner.set_callback( [this](const block_header& h){ on_mined_block( h); } );
            }

            virtual void on_mined_block( const block_header& h )
            {
               _next_block.noncea          = h.noncea;
               _next_block.nonceb          = h.nonceb;
               _next_block.timestamp       = h.timestamp;
               _next_block.next_difficulty = h.next_difficulty;
               
               _chain_client.broadcast_block( _next_block ); 
            }

            bts::blockchain::trx_block           _next_block;
            bts::net::chain_client               _chain_client;
            bts::blockchain::chain_database_ptr  _chain_db;
            bts::wallet::wallet_ptr              _wallet;
            block_miner                          _miner;
       };
    }

    client::client()
    :my( new detail::client_impl() )
    {
    }

    client::~client(){}

    void client::set_mining_effort( float e )
    {
       my->_miner.set_effort( e );
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


} } // bts::client
