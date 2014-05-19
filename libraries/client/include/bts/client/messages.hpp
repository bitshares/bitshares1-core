#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/client/client.hpp>

namespace bts { namespace client {

   enum message_type_enum
   {
      trx_message_type          = 1000,
      block_message_type        = 1001
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
      block_message(const bts::blockchain::full_block& blk )
      :block(blk),block_id(blk.id()){}

      bts::blockchain::full_block    block;
      bts::blockchain::block_id_type block_id;

   };

} } // bts::client

FC_REFLECT_ENUM( bts::client::message_type_enum, (trx_message_type)(block_message_type) )
FC_REFLECT( bts::client::trx_message, (trx) )
FC_REFLECT( bts::client::block_message, (block)(block_id) )
