#pragma once
#include <fc/reflect/reflect.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction.hpp>
#include <set>

/*
namespace bts { namespace net {

   enum chain_message_type
   {
       subscribe_msg = 1,
       block_msg     = 2,
       trx_msg       = 3,
       trx_err_msg   = 4

   };

   struct subscribe_message
   {
      static const chain_message_type type;
      uint16_t                        version;
      bts::blockchain::block_id_type  last_block;
   };

   struct block_message
   {
      static const chain_message_type type;
      block_message(){}
      block_message( const bts::blockchain::trx_block& blk )
      :block_data(blk){}

      bts::blockchain::trx_block             block_data;
   };

   struct trx_message
   {
      static const chain_message_type type;
      trx_message(){}
      trx_message( const bts::blockchain::signed_transaction& t ):signed_trx(t){}
      bts::blockchain::signed_transaction    signed_trx;                 
   };

   struct trx_err_message
   {
      static const chain_message_type type;
      bts::blockchain::signed_transaction    signed_trx;                 
      std::string                            err;
   };

} } // bts::net

FC_REFLECT_ENUM( bts::net::chain_message_type, (subscribe_msg)(block_msg)(trx_msg)(trx_err_msg) )
FC_REFLECT( bts::net::subscribe_message, (version)(last_block) )
FC_REFLECT( bts::net::block_message, (block_data) )
FC_REFLECT( bts::net::trx_message, (signed_trx) )
FC_REFLECT( bts::net::trx_err_message, (signed_trx)(err) )
*/
