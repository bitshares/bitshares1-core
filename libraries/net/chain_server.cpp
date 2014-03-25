#include <fc/crypto/ripemd160.hpp>
#include <bts/net/chain_server.hpp>
#include <bts/net/chain_connection.hpp>
#include <bts/net/chain_messages.hpp>
#include <fc/reflect/reflect.hpp>
#include <bts/net/message.hpp>
#include <bts/net/stcp_socket.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/db/level_map.hpp>
#include <fc/time.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/future.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>

#include <iostream>

#include <algorithm>
#include <unordered_map>
#include <map>


struct genesis_block_config
{
   genesis_block_config():supply(0),blockheight(0){}
   double                                            supply;
   uint64_t                                          blockheight;
   std::vector< std::pair<bts::blockchain::pts_address,uint64_t> > balances;
};
FC_REFLECT( genesis_block_config, (supply)(balances) )

using namespace bts::blockchain;

namespace bts { namespace net {
bts::blockchain::trx_block create_test_genesis_block()
{
   try {
      FC_ASSERT( fc::exists( "genesis.json" ) );
      auto config = fc::json::from_file( "genesis.json" ).as<genesis_block_config>();
      int64_t total_supply = 0;
      bts::blockchain::trx_block b;
      bts::blockchain::signed_transaction coinbase;
      coinbase.version = 0;

      uint8_t output_idx = 0;
      for( auto itr = config.balances.begin(); itr != config.balances.end(); ++itr )
      {
         total_supply += itr->second;
         ilog( "${i}", ("i",*itr) );
         coinbase.outputs.push_back( trx_output( claim_by_pts_output( itr->first ), asset( itr->second ) ) );
         if( output_idx == 0xff )
         {
            b.trxs.emplace_back( std::move(coinbase) );
            coinbase.outputs.clear();
         }
         ++output_idx;
      }

      b.version         = 0;
      b.prev            = bts::blockchain::block_id_type();
      b.block_num       = 0;
      b.total_shares    = int64_t(total_supply);
      b.timestamp       = fc::time_point::from_iso_string("20131201T054434");
      b.next_difficulty = 1;
      b.next_fee        = bts::blockchain::block_header::min_fee();
      b.available_votes = b.total_shares;

      b.trx_mroot   = b.calculate_merkle_root(signed_transactions());
      fc::variant var(b);

      auto str = fc::json::to_pretty_string(var); //b);
      ilog( "block: \n${b}", ("b", str ) );
      return b;
   } 
   catch ( const fc::exception& e )
   {
      ilog( "caught exception!: ${e}", ("e", e.to_detail_string()) );
      throw;
   }
}

namespace detail
{
   class chain_server_impl : public chain_connection_delegate
   {
      public:
        chain_server_impl()
        :ser_del(nullptr)
        {}

        ~chain_server_impl()
        {
           close();
        }
        void close()
        {
            ilog( "closing connections..." );
            try 
            {
                tcp_serv.close();
                if( accept_loop_complete.valid() )
                {
                    accept_loop_complete.cancel();
                    accept_loop_complete.wait();
                }
            } 
            catch ( const fc::canceled_exception& e )
            {
                ilog( "expected exception on closing tcp server\n" );  
            }
            catch ( const fc::exception& e )
            {
                wlog( "unhandled exception in destructor ${e}", ("e", e.to_detail_string() ));
            } 
            catch ( ... )
            {
                elog( "unexpected exception" );
            }
        }
        chain_server_delegate*                                                               ser_del;
        fc::ip::address                                                                      _external_ip;
        std::unordered_map<fc::ip::endpoint,chain_connection_ptr>                            connections;
                                                                                             
        chain_server::config                                                                 cfg;
        fc::tcp_server                                                                       tcp_serv;
                                                                                            
        fc::future<void>                                                                     accept_loop_complete;
       // fc::future<void>                                                                     block_gen_loop_complete;
        std::unordered_map<bts::blockchain::transaction_id_type,bts::blockchain::signed_transaction> pending;


       /* void block_gen_loop()
        {
             try {
                while( true )
                {
                   fc::usleep( fc::seconds(5) );

                   auto order_trxs   = chain.match_orders(); 
                   pending.insert( pending.end(), order_trxs.begin(), order_trxs.end() );
                   if( pending.size() )
                   {
                      auto new_block = chain.generate_next_block( pending );
                      pending.clear();
                      if( new_block.trxs.size() )
                      {
                        chain.push_block( new_block );
                        fc::async( [=](){ broadcast_block(new_block); } );
                      }
                   }
                }
             } 
             catch ( const fc::exception& e )
             {
                elog("${e}", ("e", e.to_detail_string()  ) );
                exit(-1);
             }
        }
        */
        void broadcast_block( const bts::blockchain::trx_block& blk )
        {
            // copy list to prevent yielding in middle...
            auto cons = connections;
            
            block_message blk_msg;
            blk_msg.block_data = blk;
            for( auto itr = cons.begin(); itr != cons.end(); ++itr )
            {
               try {
                  if( itr->second->get_last_block_id() == blk.prev )
                  {
                    itr->second->send( message( blk_msg ) );
                    itr->second->set_last_block_id( blk.id() );
                  }
               } 
               catch ( const fc::exception& w )
               {
                  wlog( "${w}", ( "w",w.to_detail_string() ) );
               }
            }
        }
        void broadcast( const message& m )
        {
            // copy list to prevent yielding in middle...
            auto cons = connections;
            ilog( "broadcast" );
            
            for( auto con : cons )
            {
               try {
                 // TODO... make sure connection is synced...
                 con.second->send( m );
               } 
               catch ( const fc::exception& w )
               {
                  wlog( "${w}", ( "w",w.to_detail_string() ) );
               }
            }
        }
                                                                   
        /**
         *  This is called every time a message is received from c, there are only two
         *  messages supported:  seek to time and broadcast.  When a message is 
         *  received it goes into the database which all of the connections are 
         *  reading from and sending to their clients.
         *
         *  The difficulty required adjusts every 5 minutes with the goal of maintaining
         *  an average data rate of 1.5 kb/sec from all connections.
         */
        virtual void on_connection_message( chain_connection& c, const message& m )
        {
             if( m.msg_type == chain_message_type::subscribe_msg )
             {
                auto sm = m.as<subscribe_message>();
                ilog( "recv: ${m}", ("m",sm) );
                c.set_last_block_id( sm.last_block );
                c.exec_sync_loop();
             }
             else if( m.msg_type == block_message::type )
             {
                try {
                   auto blk = m.as<block_message>();
                   chain.push_block( blk.block_data );
                   for( auto itr = blk.block_data.trxs.begin(); itr != blk.block_data.trxs.end(); ++itr )
                   {
                      pending.erase( itr->id() );
                   }
                   broadcast_block( blk.block_data );
                }
                catch ( const fc::exception& e )
                {
                   trx_err_message reply;
                   reply.err = e.to_detail_string();
                   wlog( "${e}", ("e", e.to_detail_string() ) );
                   c.send( message( reply ) );
                   c.close();
                }
             }
             else if( m.msg_type == chain_message_type::trx_msg )
             {
                auto trx = m.as<trx_message>();
                ilog( "recv: ${m}", ("m",trx) );
                try 
                {
                   chain.evaluate_transaction( trx.signed_trx ); // throws if error
                   if( pending.insert( std::make_pair(trx.signed_trx.id(),trx.signed_trx) ).second )
                   {
                      ilog( "new transaction, broadcasting" );
                      fc::async( [=]() { broadcast( m ); } );
                   }
                   else
                   {
                      wlog( "duplicate transaction, ignoring" );
                   }
                } 
                catch ( const fc::exception& e )
                {
                   trx_err_message reply;
                   reply.signed_trx = trx.signed_trx;
                   reply.err = e.to_detail_string();
                   wlog( "${e}", ("e", e.to_detail_string() ) );
                   c.send( message( reply ) );
                   c.close();
                }
             }
             else
             {
                 trx_err_message reply;
                 reply.err = "unsupported message type";
                 wlog( "unsupported message type" );
                 c.send( message( reply ) );
                 c.close();
             }
        }


        virtual void on_connection_disconnected( chain_connection& c )
        {
           try {
              ilog( "cleaning up connection after disconnect ${e}", ("e", c.remote_endpoint()) );
              auto cptr = c.shared_from_this();
              FC_ASSERT( cptr );
              if( ser_del ) ser_del->on_disconnected( cptr );
              auto itr = connections.find(c.remote_endpoint());
              connections.erase( itr ); //c.remote_endpoint() );
              // we cannot close/delete the connection from this callback or we will hang the fiber
              fc::async( [cptr]() {} );
           } FC_RETHROW_EXCEPTIONS( warn, "error thrown handling disconnect" );
        }

        /**
         *  This method is called via async from accept_loop and
         *  should not throw any exceptions because they are not
         *  being caught anywhere.
         *
         *  
         */
        void accept_connection( const stcp_socket_ptr& s )
        {
           try 
           {
              // init DH handshake, TODO: this could yield.. what happens if we exit here before
              // adding s to connections list.
              s->accept();
              ilog( "accepted connection from ${ep}", 
                    ("ep", std::string(s->get_socket().remote_endpoint()) ) );
              
              auto con = std::make_shared<chain_connection>(s,this);
              connections[con->remote_endpoint()] = con;
              con->set_database( &chain );
              if( ser_del ) ser_del->on_connected( con );
           } 
           catch ( const fc::canceled_exception& e )
           {
              ilog( "canceled accept operation" );
           }
           catch ( const fc::exception& e )
           {
              wlog( "error accepting connection: ${e}", ("e", e.to_detail_string() ) );
           }
           catch( ... )
           {
              elog( "unexpected exception" );
           }
        }

        /**
         *  This method is called async 
         */
        void accept_loop() throw()
        {
           try
           {
              while( !accept_loop_complete.canceled() )
              {
                 stcp_socket_ptr sock = std::make_shared<stcp_socket>();
                 tcp_serv.accept( sock->get_socket() );

                 // do the acceptance process async
                 fc::async( [=](){ accept_connection( sock ); } );

                 // limit the rate at which we accept connections to prevent
                 // DOS attacks.
                 fc::usleep( fc::microseconds( 1000*1 ) );
              }
           } 
           catch ( fc::eof_exception& e )
           {
              ilog( "accept loop eof" );
           }
           catch ( fc::canceled_exception& e )
           {
              ilog( "accept loop canceled" );
           }
           catch ( fc::exception& e )
           {
              elog( "tcp server socket threw exception\n ${e}", 
                                   ("e", e.to_detail_string() ) );
              // TODO: notify the server delegate of the error.
           }
           catch( ... )
           {
              elog( "unexpected exception" );
           }
        }
        bts::blockchain::chain_database chain;
   };
}




chain_server::chain_server()
:my( new detail::chain_server_impl() ){}

chain_server::~chain_server()
{ }


void chain_server::set_delegate( chain_server_delegate* sd )
{
   my->ser_del = sd;
}

void chain_server::configure( const chain_server::config& c )
{
  try {
     my->cfg = c;
     
     ilog( "listening for stcp connections on port ${p}", ("p",c.port) );
     my->tcp_serv.listen( c.port );
     ilog( "..." );
     my->accept_loop_complete = fc::async( [=](){ my->accept_loop(); } ); 
    // my->block_gen_loop_complete = fc::async( [=](){ my->block_gen_loop(); } ); 
     
     my->chain.open( "chain" );
     if( my->chain.head_block_num() == uint32_t(-1) )
     {
         auto genesis = create_test_genesis_block();
         ilog( "about to push" );
         try {
         //ilog( "genesis block: \n${s}", ("s", fc::json::to_pretty_string(genesis) ) );
         my->chain.push_block( genesis );
         } 
         catch ( const fc::exception& e )
         {
            wlog( "error: ${e}", ("e", e.to_detail_string() ) );
         }
         ilog( "push successful" );
     }

  } FC_RETHROW_EXCEPTIONS( warn, "error configuring server", ("config", c) );
}

std::vector<chain_connection_ptr> chain_server::get_connections()const
{ 
    std::vector<chain_connection_ptr>  cons; 
    cons.reserve( my->connections.size() );
    for( auto itr = my->connections.begin(); itr != my->connections.end(); ++itr )
    {
      cons.push_back(itr->second);
    }
    return cons;
}

/*
void chain_server::broadcast( const message& m )
{
    for( auto itr = my->connections.begin(); itr != my->connections.end(); ++itr )
    {
      try {
         itr->second->send(m);
      } 
      catch ( const fc::exception& e ) 
      {
         // TODO: propagate this exception back via the delegate or some other means... don't just fail
         wlog( "exception thrown while broadcasting ${e}", ("e", e.to_detail_string() ) );
      }
    }
}
*/

void chain_server::close()
{
  try {
    my->close();
  } FC_RETHROW_EXCEPTIONS( warn, "error closing server socket" );
}

} } // bts::net
