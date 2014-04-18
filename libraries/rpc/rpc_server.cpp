#include <bts/rpc/rpc_server.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>

namespace bts { namespace rpc { 

  namespace detail 
  {
    class rpc_server_impl
    {
       public:
         rpc_server::config  _config;
         client_ptr          _client;
         fc::tcp_server      _tcp_serv;
         fc::future<void>    _accept_loop_complete;
         rpc_server*         _self;

         /** the set of connections that have successfully logged in */
         std::unordered_set<fc::rpc::json_connection*> _login_set;

         void accept_loop()
         {
           while( !_accept_loop_complete.canceled() )
           {
              fc::tcp_socket_ptr sock = std::make_shared<fc::tcp_socket>();
              try 
              {
                _tcp_serv.accept( *sock );
              }
              catch ( const fc::exception& e )
              {
                elog( "fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string() ) );
                continue;
              }

              auto buf_istream = std::make_shared<fc::buffered_istream>( sock );
              auto buf_ostream = std::make_shared<fc::buffered_ostream>( sock );

              auto json_con = std::make_shared<fc::rpc::json_connection>( std::move(buf_istream), 
                                                                          std::move(buf_ostream) );
              register_methods( json_con );

         //   TODO  0.5 BTC: handle connection errors and and connection closed without
         //   creating an entirely new context... this is waistful
         //     json_con->exec(); 
              fc::async( [json_con]{ json_con->exec().wait(); } );
           }
         }

         void register_methods( const fc::rpc::json_connection_ptr& con )
         {
            ilog( "login!" );
            fc::rpc::json_connection* capture_con = con.get(); 
            con->add_method( "login", [=]( const fc::variants& params ) -> fc::variant 
            {
                FC_ASSERT( params.size() == 2 );
                FC_ASSERT( params[0].as_string() == _config.rpc_user )
                FC_ASSERT( params[1].as_string() == _config.rpc_password )
                _login_set.insert( capture_con );
                return fc::variant( true );
            });

            con->add_method( "transfer", [=]( const fc::variants& params ) -> fc::variant 
            {
                FC_ASSERT( _client->get_node()->is_connected() );
                check_login( capture_con );
                FC_ASSERT( params.size() == 2 );
                auto amount = params[0].as<bts::blockchain::asset>();
                auto addr   = params[1].as_string();
                auto trx    = _client->get_wallet()->transfer( amount, addr );
                _client->broadcast_transaction(trx);
                return fc::variant( trx.id() ); 
            });
            con->add_method( "getbalance", [=]( const fc::variants& params ) -> fc::variant 
            {
                check_login( capture_con );
                FC_ASSERT( params.size() == 1 );
                auto unit = params[0].as<bts::blockchain::asset_type>();
                return fc::variant( _client->get_wallet()->get_balance( unit ) ); 
            });

            con->add_method( "get_transaction", [=]( const fc::variants& params ) -> fc::variant 
            {
                FC_ASSERT( params.size() == 1 );
                return fc::variant( _client->get_chain()->fetch_transaction( params[0].as<transaction_id_type>() )  ); 
            });

            con->add_method( "getblock", [=]( const fc::variants& params ) -> fc::variant 
            {
                FC_ASSERT( params.size() == 1 );
                return fc::variant( _client->get_chain()->fetch_block( params[0].as_int64() )  ); 
            });

            con->add_method( "validateaddress", [=]( const fc::variants& params ) -> fc::variant 
            {
                FC_ASSERT( params.size() == 1 );
                try {
                   auto test = bts::blockchain::address( params[0].as_string() );
                   return fc::variant(test.is_valid());
                } 
                catch ( const fc::exception& )
                {
                  return fc::variant(false);
                }
            });

            con->add_method( "import_bitcoin_wallet", [=]( const fc::variants& params ) -> fc::variant 
            {
                check_login( capture_con );
                FC_ASSERT( params.size() == 2 );
                auto wallet_dat      = params[0].as<fc::path>();
                auto wallet_password = params[1].as_string();
                _client->get_wallet()->import_bitcoin_wallet( wallet_dat, wallet_password );
                return fc::variant(true);
            });

            _self->register_methods( con );
         } // register methods

         void check_login( fc::rpc::json_connection* con )
         {
            if( _login_set.find( con ) == _login_set.end() )
            {
               FC_THROW_EXCEPTION( exception, "not logged in" ); 
            }
         }

    };
  } // detail

  rpc_server::rpc_server()
  :my( new detail::rpc_server_impl() )
  {
     my->_self = this;
  }

  rpc_server::~rpc_server()
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

  client_ptr rpc_server::get_client()const
  {
     return my->_client;
  }

  void rpc_server::set_client( const client_ptr& c )
  {
     my->_client = c;
  }

  void rpc_server::configure( const rpc_server::config& cfg )
  {
     try {
       my->_config = cfg;
       my->_tcp_serv.listen( cfg.rpc_endpoint );
       ilog( "listening for rpc connections on port ${port}", ("port",my->_tcp_serv.get_port()) );
     
       my->_accept_loop_complete = fc::async( [=]{ my->accept_loop(); } );
     
     } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.rpc_endpoint)("config",cfg) );
  }

  /**
   *  Adds the JSON-RPC methods to the connection.  This method should be overridden to add 
   *  additional methods for customized DACs.
   */
  void rpc_server::register_methods( const fc::rpc::json_connection_ptr& con )
  {
  }

  void rpc_server::check_login( fc::rpc::json_connection* con )
  {
     my->check_login( con );
  }


} } // bts::rpc
