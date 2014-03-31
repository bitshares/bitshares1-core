#pragma once
#include <bts/kid/name_record.hpp>

namespace fc 
{ 
   class path; 
   class sha256;
   namespace ip { class endpoint; }
} // namespace fc

namespace bts { namespace kid
{
   namespace detail { class server_impl; }

   struct name_index
   {
      name_index( uint32_t bn = 0, uint16_t rn = 0)
      :block_num(bn),record_num(rn){}

      uint32_t block_num;
      uint16_t record_num;
   };
   struct history
   {
      std::vector<name_index> updates;
   };

   class server
   {
      public:
         server();
         ~server();

         void set_trustee( const fc::ecc::private_key& k );
         void set_data_directory( const fc::path& dir );
         void listen( const fc::ip::endpoint& ep );

         bool                    update_record( const signed_name_record& r );
         signed_name_record      fetch_record_by_key( const std::string& name_b58 );
         signed_name_record      fetch_record( const std::string& name );
         signed_name_record      fetch_record( const name_index& n );
         history                 fetch_history( const std::string& name );

         signed_block            fetch_block( uint32_t block_num );
         signed_block            head_block()const;
         fc::sha256              head_block_id()const;

         void                    store_key( const std::string& name, const stored_key& key );
         stored_key              fetch_key( const std::string& name );

      private:
         std::unique_ptr<detail::server_impl> my;
   };
} } // namespace bts::kid

FC_REFLECT( bts::kid::name_index, (block_num)(record_num) )
FC_REFLECT( bts::kid::history, (updates) )
