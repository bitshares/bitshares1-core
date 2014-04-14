#pragma once
#include <bts/blockchain/chain_database.hpp>

using namespace bts::blockchain;

namespace bts { namespace net {

   namespace detail { class chain_client_impl; }

   class chain_client_delegate
   {
      public:
         virtual ~chain_client_delegate(){}
         virtual void on_new_block( const trx_block& block ){}
         virtual void on_new_transaction( const signed_transaction& trx ){}
   };

   class chain_client
   {
      public:
         chain_client();
         ~chain_client();

         void set_delegate( chain_client_delegate* del );
         void set_chain( const chain_database_ptr& chain );
         void add_node( const std::string& ep );

         void broadcast_transaction( const signed_transaction& trx );
         void broadcast_block( const trx_block& blk );
         bool is_connected()const;


      private:
         std::unique_ptr<detail::chain_client_impl> my;
   };

   typedef std::shared_ptr<chain_client> chain_client_ptr;


} } // bts::net
