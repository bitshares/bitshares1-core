#pragma once
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   struct certified_transaction
   {
      certified_transaction():valid(false){}

      fc::sha256 digest()const;

      signed_transaction trx;
      /** true if trx was valid at time specified by timestamp, otherwise false */
      bool               valid;
      /** the time that trx was validated by the delegate */
      fc::time_point_sec timestamp;
   };
   
   struct signed_certified_transaction : public certified_transaction
   {
      fc::ecc::public_key signee()const;
      fc::ecc::compact_signature delegate_signature;
   };

   struct signed_header_conflict
   {
      signed_block_header first;
      signed_block_header second;
   };


} }

FC_REFLECT( bts::blockchain::certified_transaction, (trx)(valid)(timestamp) )
FC_REFLECT_DERIVED( bts::blockchain::signed_certified_transaction, 
                    (bts::blockchain::certified_transaciton), 
                    (trx)(valid)(timestamp) )
FC_REFLECT( signed_header_conflict, (first)(second) )
