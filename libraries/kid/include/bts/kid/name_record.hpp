#include <fc/crypto/elliptic.hpp>
#include <fc/time.hpp>
#include <fc/reflect/reflect.hpp>

namespace bts { namespace kid 
{
   struct name_record
   {
       name_record():nonce(0){}

       fc::sha256 digest()const;
       fc::sha256 digest512()const;
       uint64_t   difficulty()const;
 
       std::string         name;
       fc::ecc::public_key master_key;
       fc::ecc::public_key active_key;
       fc::sha256          prev_block_id;
       fc::time_point_sec  last_update;
       fc::time_point_sec  first_update;
       uint32_t            nonce; // used to rate limit
   };
 
   struct signed_name_record : public name_record
   {
      fc::sha256 id()const;
      fc::ecc::public_key get_signee()const;
      void                sign( const fc::ecc::private_key& master_key );
      fc::ecc::compact_signature master_signature;
   };
 
   struct block
   {
      block():number(0),difficulty(1000){}
      fc::sha256 digest()const;

      uint32_t                          number;
      fc::time_point_sec                timestamp;
      uint64_t                          difficulty;
      std::vector<signed_name_record>   records;
   };

   struct signed_block : public block
   {
      fc::sha256 id()const;
      void       sign( const fc::ecc::private_key& trustee_priv_key );
      void       verify( const fc::ecc::public_key& trustee_pub_key );

      fc::ecc::compact_signature trustee_signature;
   };

   struct stored_key
   {
       fc::ecc::public_key get_signee()const;
       std::vector<char>          encrypted_key;
       fc::ecc::compact_signature signature;
   };
} }  // namespace bts::kid

FC_REFLECT( bts::kid::name_record, (name)(master_key)(active_key)(prev_block_id)(last_update)(first_update)(nonce) )
FC_REFLECT_DERIVED( bts::kid::signed_name_record, (bts::kid::name_record), (master_signature) )
FC_REFLECT( bts::kid::block, (number)(timestamp)(difficulty)(records) )
FC_REFLECT_DERIVED( bts::kid::signed_block, (bts::kid::block), (trustee_signature) )
FC_REFLECT( bts::kid::stored_key, (encrypted_key)(signature) )
