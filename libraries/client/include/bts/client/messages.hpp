#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/client/client.hpp>

namespace bts { namespace client {

   enum message_type_enum
   {
      trx_message_type          = 1000,
      block_message_type        = 1001,
      signature_message_type    = 1002,
      notification_message_type = 1003
   };

   struct trx_message
   {
      static const message_type_enum type;

      bts::blockchain::signed_transaction trx;
      trx_message() {}
      trx_message(bts::blockchain::signed_transaction transaction) :
        trx(std::move(transaction))
      {}
   };

   struct block_message
   {
      static const message_type_enum type;

      block_message(){}
      block_message( const blockchain::block_id_type& id, const bts::blockchain::full_block& blk, const fc::ecc::compact_signature& s )
      :block_id(id),block(blk),signature(s){}

      blockchain::block_id_type  block_id;
      bts::blockchain::full_block block;
      fc::ecc::compact_signature signature;
   };

   struct signature_message
   {
      static const message_type_enum type;

      blockchain::block_id_type    block_id;
      fc::ecc::compact_signature   signature;
   };

  struct client_notification_message
  {
    static const message_type_enum type;
    client_notification notification;
  };

} } // bts::client

FC_REFLECT_ENUM( bts::client::message_type_enum, (trx_message_type)(block_message_type)(signature_message_type)(notification_message_type) )
FC_REFLECT( bts::client::trx_message, (trx) )
FC_REFLECT( bts::client::block_message, (block_id)(block)(signature) )
FC_REFLECT( bts::client::signature_message, (block_id)(signature) )
FC_REFLECT( bts::client::client_notification_message, (notification) )
