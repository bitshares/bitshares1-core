#include <bts/net/node.hpp>
#include <bts/net/stcp_socket.hpp>
#include <bts/net/connection.hpp>

namespace bts { namespace net {

   namespace detail {

      class connection
      {
         public:
            connection( const stcp_socket& sock, node_impl& n )
            :_sock(sock),_node(n){}

            stcp_socket_ptr _sock;
            node_impl&      _node;
      };
      typedef std::shared_pointer<connection> connection_ptr;

      class node_impl
      {
         public:
            node_delegate*      _delegate;
            fc::tcp_server      _tcp_serv;
            fc::future<void>    _accept_loop_complete;

            /** Stores all connections which are in the process of being connected */
            std::unordered_map<stcp_socket_ptr, fc::future<void> > _pending_connections;
            std::unordered_set<connection_ptr>                     _active_connections;


            node_impl()
            :_delegate( nullptr ){}

            ~node_impl()
            {
                try { 
                   close(); 
                } 
                catch( const fc::exception& e )
                {
                   wlog( "unexpected exception on close ${e}", ("e", e.to_detail_string() ) );
                }
            }

            void close()
            {
               try {
                  _tcp_serv.close();
                  if( accept_loop_complete.valid() )
                  {
                      accept_loop_complete.cancel();
                      accept_loop_complete.wait();
                  }
               } 
               catch( const fc::exception& e )
               {
                  wlog( "unhandled exception ${e}", ("e",e.to_detail_string()) );
               }
               

            }


            void accept_connection( const stcp_socket_ptr& sock )
            {
               try {
                  sock->accept(); // perform key exchange

                  auto con = std::make_shared<connection>( sock, std::ref(*this) );
                  _active_connections.insert(con);
               } 
               catch ( const fc::exception& e )
               {
                   wlog( "error accepting connection ${e}", ("e", e.to_detail_string() ) );
               }
               _pending_connections.erase( sock );
            }

            void accept_loop()
            {
                while( !_accept_loop_complete.canceled() )
                {
                   stcp_socket_ptr sock = std::make_shared<stcp_socket>();
                   try {
                       _tcp_serv.accept( sock->get_socket() );

                       _pending_connections[sock] = fc::async( [=](){ accept_connection( sock ); } );

                       // limit the rate at which we accept connections to mitigate DOS attacks
                       fc::usleep( fc::microseconds( 1000*10 ) );
                   }
                   catch ( const fc::exception& e )
                   {
                     elog( "fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string() ) );
                     throw;
                   }
                }
            } // accept_loop
      };

   } // namespace detail

   node::node()
   :my( new detail::node_impl() )
   {
   }

   node::~node()
   {
       try {
           my->_tcp_serv.close();
           if( my->_accept_loop_complete.valid() )
           {
              my->_accept_loop_complete.cancel();
              my->_accept_loop_complete.wait();
           }
       } 
       catch ( const fc::canceled_exception& e ){}
       catch ( const fc::exception& e )
       {
          wlog( "unhandled exception thrown in destructor.\n${e}", ("e", e.to_detail_string() ) );
       }
   }

   void node::set_delegate(  node_delegate* del )
   {
      my->_delegate = del;
   }


   void  node::add_node( const fc::ip::endpoint& ep )
   {
   }

   void  node::connect_to( const fc::ip::endpoint& ep )
   {
   }

   void  node::listen_on_endpoint( const fc::ip::endpoint& ep )
   { try {
       my->_tcp_serv.listen( ep );
       my->_accept_loop_complete = fc::async( [=](){ my->accept_loop(); } );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to listen on ${ep}", ("ep",ep) ) }

   void node::broadcast( const message& msg )
   {

   }

   void node::sync_from( const item_id& id )
   {
   }

   message node::fetch( const item_id& id )
   {

   }

} } // bts::net
