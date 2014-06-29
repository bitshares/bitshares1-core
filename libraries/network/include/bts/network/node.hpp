#pragma once
#include <bts/network/peer_connection.hpp>

#include <bts/blockchain/types.hpp>
#include <bts/client/client.hpp>
#include <fc/network/udt_socket.hpp>
#include <unordered_set>

namespace bts { namespace network {

   using namespace bts::client;

   struct peer_status : public peer_info
   {
       peer_status()
       :failed_attempts(0),is_connected(false){}

       fc::time_point last_connect_attempt;
       int32_t        failed_attempts;
       bool           is_connected;
   };

   class node
   {
      public:
         node();
         ~node();

         void set_client( client_ptr c );

         void listen( const fc::ip::endpoint& ep );
         void connect_to( const fc::ip::endpoint& ep );

         void signin( const vector<fc::ecc::private_key>& keys );

         vector<peer_info> get_peers()const;

         void set_desired_connections( int32_t count );

         void broadcast_block( const full_block& block_to_broadcast );

      private:
         void attempt_new_connection();

         void accept_loop();
         void broadcast_keep_alive();

         void on_connect( const shared_ptr<peer_connection>& con  );
         void on_disconnect( const shared_ptr<peer_connection>& con, 
                             const optional<fc::exception>& err );

         void on_received_message( const shared_ptr<peer_connection>&, 
                                   const message& );

         void on_signin_request( const shared_ptr<peer_connection>&, 
                                 const signin_request& r );

         void on_goodbye( const shared_ptr<peer_connection>&, 
                          const goodbye_message& r );

         void on_peer_request( const shared_ptr<peer_connection>& con, 
                               const peer_request_message& request );

         void on_peer_response( const shared_ptr<peer_connection>& con, 
                                const peer_request_message& request );

         void on_block_message( const shared_ptr<peer_connection>& con, 
                                const block_message& request );

         map<fc::ip::endpoint, peer_status>    _potential_peers;
         
         int32_t                               _desired_peer_count;
         fc::future<void>                      _accept_loop;
         fc::future<void>                      _keep_alive_task;
         fc::future<void>                      _attempt_new_connections_task;
         client_ptr                            _client;
         fc::udt_server                        _server;
         unordered_set< peer_connection_ptr >  _peers;
         vector<fc::ecc::private_key>          _signin_keys;
   };

} } 

