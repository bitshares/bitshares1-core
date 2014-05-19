#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/block.hpp>

namespace bts { namespace blockchain {
   struct delegate_testimony
   {
      digest_type         digest()const;

      name_id_type        delegate_id;
      transaction_id_type transaction_id;
      bool                valid;
      fc::time_point_sec  timestamp;
   };

   struct signed_delegate_testimony : public delegate_testimony
   {
      void             sign( const fc::ecc::private_key& k );
      public_key_type  signee()const;

      signature_type delegate_signature;
   };

   typedef std::pair<signed_block_header,signed_block_header> multiple_block_proof;

   struct fire_delegate_operation
   {
      static const operation_type_enum type; 

      enum reason_type 
      {
         /** delegate signed two blocks in the same timestamp */
         multiple_blocks_signed   = 0, 
         invalid_testimony        = 1
      };

      fire_delegate_operation():delegate_id(0){}
      fire_delegate_operation( name_id_type delegate_id, const multiple_block_proof& p );
      fire_delegate_operation( const signed_delegate_testimony& p );

      fc::enum_type<uint8_t,reason_type> reason;
      name_id_type                       delegate_id;
      std::vector<char>                  data;
   };

} } // bts::blockchain

#include <fc/reflect/reflect.hpp>
FC_REFLECT_TYPENAME( bts::blockchain::multiple_block_proof )
FC_REFLECT_ENUM( bts::blockchain::fire_delegate_operation::reason_type, (multiple_blocks_signed)(invalid_testimony) )
FC_REFLECT( bts::blockchain::delegate_testimony, (delegate_id)(transaction_id)(valid)(timestamp) )
FC_REFLECT_DERIVED( bts::blockchain::signed_delegate_testimony, (bts::blockchain::delegate_testimony), (delegate_signature) )
FC_REFLECT( bts::blockchain::fire_delegate_operation, (reason)(delegate_id)(data) )
