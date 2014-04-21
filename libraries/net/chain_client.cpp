#include <bts/net/chain_client.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/net/chain_connection.hpp>
#include <bts/net/chain_messages.hpp>
#include <fc/thread/thread.hpp>
#include <fc/reflect/variant.hpp>

#include <iostream>


using namespace bts::blockchain;


namespace bts { namespace net {

   namespace detail {

   class chain_client_impl : public chain_connection_delegate
   {
      public:
        virtual void on_connection_message( chain_connection& c, const message& m )
        {
            if( m.msg_type == chain_message_type::block_msg )
            {
               auto blkmsg = m.as<block_message>();
               ilog( "received block num ${n}", ("n",blkmsg.block_data.block_num) );
               _delegate->on_new_block( blkmsg.block_data );
            }
            else if( m.msg_type == trx_message::type )
            {
               auto trx_msg = m.as<trx_message>();
               ilog( "received message ${m}", ("m",trx_msg) );
               _delegate->on_new_transaction( trx_msg.signed_trx );
            }
            else if( m.msg_type == trx_err_message::type )
            {
               auto errmsg = m.as<trx_err_message>();
               std::cerr<<  errmsg.err <<"\n";
               elog( "${e}", ("e", errmsg ) );
            }
        }
        
        virtual void on_connection_disconnected( chain_connection& c )
        {
            start_connect_loop();
        }
        void start_connect_loop()
        {
             _chain_connect_loop_complete = fc::async( 
                   [this](){ fc::usleep(fc::seconds(1)); chain_connect_loop(); } );
        }

        void chain_connect_loop()
        {
            _chain_connected = false;
            FC_ASSERT( _unique_node_list.size() > 0 );
            while( !_chain_connect_loop_complete.canceled() )
            {
               // attempt to connect to all nodes.
               for( auto ep : _unique_node_list )
               {
                    try {
                       // std::cout<< "\rconnecting to bitshares network: "<<std::string(ep)<<"\n";
                       // TODO: pass public key to connection so we can avoid man-in-the-middle attacks
                       _chain_con.connect( fc::ip::endpoint::from_string(ep) );

                       subscribe_message msg;
                       msg.version        = 0;
                       if( _chain->head_block_num() != uint32_t(-1) )
                       {
                          msg.last_block     = _chain->head_block_id();
                       }
                       _chain_con.send( message( msg ) );
                       // std::cout<< "\rconnected to bitshares network\n";
                       _chain_connected = true;
                       return;
                    } 
                    catch ( const fc::exception& e )
                    {
                       std::cout<< "\nunable to connect to bitshares network at this time.\n";
                       wlog( "${e}", ("e",e.to_detail_string()));
                    }
               }

               // sleep in .5 second increments so we can quit quickly without hanging
               for( uint32_t i = 0; i < 30 && !_chain_connect_loop_complete.canceled(); ++i )
                  fc::usleep( fc::microseconds(500*1000) );
            }
        }

        chain_client_impl():_delegate(nullptr),_chain_con(this),_chain_connected(false){}


        chain_client_delegate*                                     _delegate;
        chain_connection                                           _chain_con;
        bool                                                       _chain_connected;

        std::vector<std::string>                                   _unique_node_list;

        chain_client_delegate*                                     _chain_client;
        chain_database_ptr                                         _chain;
        fc::future<void>                                           _chain_connect_loop_complete;
   };

  } // namespace detail

  chain_client::chain_client()
  :my( new detail::chain_client_impl() ){}

  chain_client::~chain_client(){}

  void chain_client::set_delegate( chain_client_delegate* del )
  {
     my->_delegate = del;
  }
  void chain_client::set_chain( const chain_database_ptr& chain )
  {
     my->_chain = chain;
  }
  void chain_client::add_node( const std::string& ep )
  {
     my->_unique_node_list.push_back(ep);
     if( !is_connected() )
        my->start_connect_loop();
  }

  void chain_client::broadcast_transaction( const signed_transaction& trx )
  {
     // my->_pending_trxs[trx.id()] = trx;
     my->_chain_con.send( trx_message( trx ) );
  }

  void chain_client::broadcast_block( const trx_block& blk )
  {
     my->_chain_con.send( block_message( blk ) );
  }

  bool chain_client::is_connected()const
  {
     return my->_chain_connected;
  }


} } // bts::net
