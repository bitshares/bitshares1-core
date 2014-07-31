#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace mail {

   typedef fc::ripemd160 message_id_type;

   /**
    * negitive types are not encrypted
    */
   enum message_type
   {
       market_notice        = -1, // not encrypted
       encrypted            = 0,
       transaction_notice   = 1,
       email                = 3
   };

   struct message;

   struct encrypted_message
   {
      message decrypt( const fc::ecc::private_key& e )const;

      fc::ecc::public_key  onetimekey;
      std::vector<char>    data;
   };

   struct message
   {
      message():nonce(0){}

      message_id_type id()const;
      encrypted_message encrypt( const fc::ecc::private_key& onetimekey,
                                 const fc::ecc::public_key&  receiver_public_key )const;

      template<typename MessageType>
      message( const MessageType& m )
      {
         type = MessageType::type;
         data = fc::raw::pack(m);
      }

      template<typename MessageType>
      MessageType as()const
      {
         FC_ASSERT( type == MessageType::type, "", ("type",type)("MessageType",MessageType::type) ) 
         return fc::raw::unpack<MessageType>(data);
      }

      fc::enum_type<int16_t, message_type>  type;
      uint64_t                              nonce;
      std::vector<char>                     data;
   };

} } // bts::mail

FC_REFLECT_ENUM( bts::mail::message_type, (encrypted)(transaction_notice)(market_notice)(email) )
FC_REFLECT( bts::mail::encrypted_message, (onetimekey)(data) )
FC_REFLECT( bts::mail::message, (type)(nonce)(data) )
