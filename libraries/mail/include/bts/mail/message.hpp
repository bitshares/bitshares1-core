#pragma once
#include <bts/blockchain/transaction.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace mail {

   typedef fc::ripemd160 message_id_type;
   typedef fc::sha256    digest_type;
   using std::string;
   using std::vector;

   struct attachment
   {
      string       name;
      vector<char> data;
   };

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

   struct transaction_notice_message
   {
      static const message_type type;

      bts::blockchain::signed_transaction trx;
      string                              extended_memo;
   };

   struct email_message
   {
      digest_type            digest()const;
      string                 subject;
      string                 body;
      vector< attachment >   attachments;
   };

   struct signed_email_message : public email_message
   {
      static const message_type type;

      void                sign( const fc::ecc::private_key& from_key );
      fc::ecc::public_key from()const;

      fc::ecc::compact_signature from_signature; 
   };

   struct message;

   struct encrypted_message
   {
      static const message_type type;

      message decrypt( const fc::ecc::private_key& e )const;
      message decrypt( const fc::sha512& shared_secret )const;

      fc::ecc::public_key  onetimekey;
      vector<char>         data;
   };

   struct message
   {
      message():nonce(0),timestamp(fc::time_point::now()){}

      message_id_type   id()const;
      encrypted_message encrypt( const fc::ecc::private_key& onetimekey,
                                 const fc::ecc::public_key&  receiver_public_key )const;

      template<typename MessageType>
      message( const MessageType& m )
      {
         type = MessageType::type;
         timestamp = fc::time_point::now();
         data = fc::raw::pack(m);
      }

      template<typename MessageType>
      MessageType as()const
      {
         FC_ASSERT( type == MessageType::type, "", ("type",type)("MessageType",MessageType::type) );
         return fc::raw::unpack<MessageType>(data);
      }

      fc::enum_type<int16_t, message_type>  type;
      uint64_t                              nonce;
      fc::time_point_sec                    timestamp;
      vector<char>                          data;
   };

} } // bts::mail

FC_REFLECT_ENUM( bts::mail::message_type, (encrypted)(transaction_notice)(market_notice)(email) )
FC_REFLECT( bts::mail::encrypted_message, (onetimekey)(data) )
FC_REFLECT( bts::mail::message, (type)(nonce)(timestamp)(data) )
FC_REFLECT( bts::mail::attachment, (name)(data) )
FC_REFLECT( bts::mail::transaction_notice_message, (trx)(extended_memo) )
FC_REFLECT( bts::mail::email_message, (subject)(body)(attachments) )
FC_REFLECT_DERIVED( bts::mail::signed_email_message, (bts::mail::email_message), (from_signature) )

