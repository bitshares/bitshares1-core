#include "chain_connection.hpp"
#include "chain_messages.hpp"
#include <bts/net/message.hpp>
#include <bts/blockchain/config.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/network/resolve.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>
#include <fc/string.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <unordered_map>
#include <bts/db/level_map.hpp>

const chain_message_type subscribe_message::type = chain_message_type::subscribe_msg;
const chain_message_type block_message::type     = chain_message_type::block_msg;
const chain_message_type trx_message::type       = chain_message_type::trx_msg;
const chain_message_type trx_err_message::type   = chain_message_type::trx_err_msg;

  namespace detail
  {
     class chain_connection_impl
     {
        public:
          chain_connection_impl(chain_connection& s)
          :self(s),con_del(nullptr){}
          chain_connection&          self;
          stcp_socket_ptr      sock;
          fc::ip::endpoint     remote_ep;
          chain_connection_delegate* con_del;

          bts::blockchain::block_id_type   _last_block_id;
          bts::blockchain::chain_database* chain;

          /** used to ensure that messages are written completely */
          fc::mutex              write_lock;


          fc::future<void>       read_loop_complete;
          fc::future<void>       exec_sync_loop_complete;

          void read_loop()
          {
            const int BUFFER_SIZE = 16;
            const int LEFTOVER = BUFFER_SIZE - sizeof(message_header);
            try {
               message m;
               while( !read_loop_complete.canceled() )
               {
                  char tmp[BUFFER_SIZE];
                  ilog( "read.." );
                  sock->read( tmp, BUFFER_SIZE );
                  ilog( "." );
                  memcpy( (char*)&m, tmp, sizeof(message_header) );
                  m.data.resize( m.size + 16 ); //give extra 16 bytes to allow for padding added in send call
                  memcpy( (char*)m.data.data(), tmp + sizeof(message_header), LEFTOVER );
                  sock->read( m.data.data() + LEFTOVER, 16*((m.size -LEFTOVER + 15)/16) );

                  try { // message handling errors are warnings... 
                    con_del->on_connection_message( self, m );
                  } 
                  catch ( fc::canceled_exception& e ) { wlog(".");throw; }
                  catch ( fc::eof_exception& e ) { wlog(".");throw; }
                  catch ( fc::exception& e ) 
                  { 
                     wlog( "disconnected ${er}", ("er", e.to_detail_string() ) );
                     return;
                     // TODO: log and potentiall disconnect... for now just warn.
                  }
                  catch( ... )
                  {
                     wlog("...????" );
                     return;
                  }
               }
            } 
            catch ( const fc::canceled_exception& e )
            {
              if( con_del )
              {
                 con_del->on_connection_disconnected( self );
              }
              else
              {
                wlog( "disconnected ${e}", ("e", e.to_detail_string() ) );
              }
              wlog( "exit read loop" );
              return;
            }
            catch ( const fc::eof_exception& e )
            {
              if( con_del )
              {
                 ilog( ".");
                 fc::async( [=](){con_del->on_connection_disconnected( self );}, "on_connection_disconnected" );
                 ilog( ".");
              }
              else
              {
                wlog( "disconnected ${e}", ("e", e.to_detail_string() ) );
              }
            }
            catch ( fc::exception& er )
            {
               wlog( ".." );
              if( con_del )
              {
                elog( "disconnected ${er}", ("er", er.to_detail_string() ) );
                //con_del->on_connection_disconnected( self );
                fc::async( [=](){con_del->on_connection_disconnected( self );}, "exception on_connection_disconnected" );
              }
              else
              {
                elog( "disconnected ${e}", ("e", er.to_detail_string() ) );
              }
              FC_RETHROW_EXCEPTION( er, warn, "disconnected ${e}", ("e", er.to_detail_string() ) );
            }
            catch ( ... )
            {
               wlog( "unhandled??" );
              // TODO: call con_del->????
              FC_THROW_EXCEPTION( unhandled_exception, "disconnected: {e}", ("e", fc::except_str() ) );
            }
          }
     };
  } // namespace detail

  chain_connection::chain_connection( const stcp_socket_ptr& c, chain_connection_delegate* d )
  :my( new ::detail::chain_connection_impl(*this) )
  {
    my->sock = c;
    my->con_del = d;
    my->remote_ep = remote_endpoint();
    my->read_loop_complete = fc::async( [=](){ my->read_loop(); }, "chain_connection::read_loop" );
  }

  chain_connection::chain_connection( chain_connection_delegate* d )
  :my( new ::detail::chain_connection_impl(*this) ) 
  { 
    assert( d != nullptr );
    my->con_del = d; 
  }

  void chain_connection::set_last_block_id( const bts::blockchain::block_id_type& id )
  {
     my->_last_block_id = id;
  }
  bts::blockchain::block_id_type chain_connection::get_last_block_id()const
  {
     return my->_last_block_id;
  }

  chain_connection::~chain_connection()
  {
    try {
        // delegate does not get called from destructor...
        // because shared_from_this() will return nullptr 
        // and cause us all kinds of grief
        my->con_del = nullptr; 

        close();
        if( my->read_loop_complete.valid() )
        {
          my->read_loop_complete.wait();
        }
        if( my->exec_sync_loop_complete.valid() )
        {
          my->exec_sync_loop_complete.cancel();
          my->exec_sync_loop_complete.wait();
        }
    } 
    catch ( const fc::canceled_exception& e )
    {
      ilog( "canceled" );
    }
    catch ( const fc::exception& e )
    {
      wlog( "unhandled exception on close:\n${e}", ("e", e.to_detail_string()) );   
    }
    catch ( ... )
    {
      elog( "unhandled exception on close ${e}", ("e", fc::except_str()) );   
    }
  }
  stcp_socket_ptr chain_connection::get_socket()const
  {
     return my->sock;
  }

  void chain_connection::close()
  {
     try {
         if( my->sock )
         {
           my->sock->close();
           if( my->read_loop_complete.valid() )
           {
              wlog( "waiting for socket to close" );
              my->read_loop_complete.cancel();
              try {
                 my->read_loop_complete.wait();
              } catch ( const fc::exception& e )
              {
                 wlog( "${w}", ("w",e.to_detail_string()) );
              } catch ( ...) {}
              wlog( "socket closed" );
           }
         }
     } FC_RETHROW_EXCEPTIONS( warn, "exception thrown while closing socket" );
  }
  void chain_connection::connect( const fc::ip::endpoint& ep )
  {
     try {
       // TODO: do we have to worry about multiple calls to connect?
       my->sock = std::make_shared<stcp_socket>();
       my->sock->connect_to(ep); 
       my->remote_ep = remote_endpoint();
       ilog( "    connected to ${ep}", ("ep", ep) );
       my->read_loop_complete = fc::async( [=](){ my->read_loop(); } );
     } FC_RETHROW_EXCEPTIONS( warn, "error connecting to ${ep}", ("ep",ep) );
  }

  void chain_connection::connect( const std::string& host_port )
  {
      int idx = host_port.find( ':' );
      auto eps = fc::resolve( host_port.substr( 0, idx ), fc::to_int64(host_port.substr( idx+1 )));
      ilog( "connect to ${host_port} and resolved ${endpoints}", ("host_port", host_port)("endpoints",eps) );
      for( auto itr = eps.begin(); itr != eps.end(); ++itr )
      {
         try 
         {
            connect( *itr );
            return;
         } 
         catch ( const fc::exception& e )
         {
            wlog( "    attempt to connect to ${ep} failed.", ("ep", *itr) );
         }
      }
      FC_THROW_EXCEPTION( exception, "unable to connect to ${host_port}", ("host_port",host_port) );
  }

  void chain_connection::send( const message& m )
  {
    try {
      fc::scoped_lock<fc::mutex> lock(my->write_lock);
#define MAIL_PACKED_MESSAGE_HEADER sizeof(message_header)
      size_t len = MAIL_PACKED_MESSAGE_HEADER + m.size;
      len = 16*((len+15)/16); //pad the message we send to a multiple of 16 bytes
      std::vector<char> tmp(len);
      memcpy( tmp.data(), (char*)&m, MAIL_PACKED_MESSAGE_HEADER );
      memcpy( tmp.data() + MAIL_PACKED_MESSAGE_HEADER, m.data.data(), m.size );
      my->sock->write( tmp.data(), tmp.size() );
      my->sock->flush();
    } FC_RETHROW_EXCEPTIONS( warn, "unable to send message" );
  }


  fc::ip::endpoint chain_connection::remote_endpoint()const 
  {
     if( get_socket()->get_socket().is_open() )
     {
         return my->remote_ep = get_socket()->get_socket().remote_endpoint();
     }
     // TODO: document why we are not throwing an exception if there is no remote endpoint?
     return my->remote_ep;
  }

  void chain_connection::exec_sync_loop()
  {
      my->exec_sync_loop_complete = fc::async( [=]() 
      {
          while( !my->exec_sync_loop_complete.canceled() )
          {
             try {
                int32_t  cur_block_num  = int32_t(-1);
                if( my->_last_block_id != bts::blockchain::block_id_type() )
                    cur_block_num = my->chain->fetch_block_num( my->_last_block_id );
                ilog( "head block ${h}  cur block ${c}", ("c",cur_block_num)("h",my->chain->head_block_num() ) );
                while( cur_block_num < int32_t(my->chain->head_block_num())  )
                {
                    cur_block_num++;
                    block_message blk_msg;
                    blk_msg.block_data = my->chain->fetch_trx_block( cur_block_num );
                    // TODO: sign it..
                    ilog( "sending block ${n} ${c}", ("n",cur_block_num)("c",blk_msg.block_data.id()) );
                    send( bts::net::message(blk_msg) );
                    my->_last_block_id = blk_msg.block_data.id();
                    fc::usleep( fc::microseconds( 1000*100 ) );
                }
                ilog( "all synced up, no blocks left to send" );
                return;
             } 
             catch ( const fc::exception& e ) 
             {
                wlog( "${e}", ("e", e.to_detail_string() ) );
                trx_err_message reply;
                reply.err = e.to_detail_string();
                send( message( reply ) );
                get_socket()->get_socket().close();
                return;
             }
          }
      });
  }

  void chain_connection::set_database( bts::blockchain::chain_database* db )
  {
     my->chain = db;
  }


