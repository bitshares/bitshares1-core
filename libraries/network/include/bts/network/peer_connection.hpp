#pragma once
#include <bts/network/messages.hpp>
#include <fc/network/udt_socket.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/signals.hpp>
#include <fc/optional.hpp>

#include <deque>

namespace bts { namespace network {

   using namespace std;
   using namespace bts::blockchain;
   using fc::time_point;
   using bts::blockchain::public_key_type;
   using fc::ip::endpoint;

   class node;

   class peer_connection : public enable_shared_from_this<peer_connection>
   {
      public:
         peer_connection();
         ~peer_connection();

         peer_info info;

         void start_read_loop();

         void send_message( const message& );

         void signin( const fc::ecc::private_key& k );

         void request_peers();

         fc::signal<void( const shared_ptr<peer_connection>&,  
                          const optional<fc::exception>& )>  connection_closed;

         fc::signal<void( const shared_ptr<peer_connection>&,  
                          const message& )>                  received_message;

         fc::ip::endpoint remote_endpoint();
      private:
         friend class node;
         void read_loop();
         bool is_logged_in()const;
         void process_send_queue();

         fc::sha512                       _shared_secret;
         fc::future<void>                 _read_loop;
         fc::udt_socket                   _socket;
         optional<fc::ip::endpoint>       _remote_endpoint;
         fc::future<void>                 _send_queue_complete;
         std::deque<message>              _send_queue;
   };

   typedef shared_ptr<peer_connection> peer_connection_ptr;

} }

