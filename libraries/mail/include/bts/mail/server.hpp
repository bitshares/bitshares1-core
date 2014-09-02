#pragma once
#include <bts/blockchain/address.hpp>
#include <bts/mail/message.hpp>
#include <bts/mail/config.hpp>

#include <map>

namespace bts { namespace mail {

   namespace detail { class server_impl; }

   typedef std::vector< std::pair< fc::time_point, message_id_type > > inventory_type;

   /**
    *  The mail server is designed to facilitate light weight clients and provide them
    *  with the information they need to use the blockchain.  When a user broadcasts
    *  a transaction they will also post a notification message to the mail server for
    *  the user.   The notification should be posted after the transaction has
    *  been reported as confirmed.
    *
    *  mail_store( owner, message )
    *  mail_fetch_inventory( owner, start_time, limit ) => vector<message_id_type>
    *  mail_fetch_message( message_id_type )
    */
    class server : public std::enable_shared_from_this<server>
    {
       public:
          server();
          ~server();

          void open( const fc::path& data_dir );
          void close();
          
          void store( const bts::blockchain::address& owner, 
                      const message& msg );

          inventory_type fetch_inventory( const bts::blockchain::address& owner, 
                                          const fc::time_point& start, 
                                          uint32_t limit = BTS_MAIL_INVENTORY_FETCH_LIMIT )const;
          message fetch_message( const message_id_type& inventory_id )const;

       private:
          std::unique_ptr<detail::server_impl> my;
    };

    typedef std::shared_ptr<server> mail_server_ptr;

} } // bts::mail
