#pragma once
#include <bts/client/client.hpp>

namespace bts { namespace cli {
   
   using namespace client;

   namespace detail {  class cli_impl; }

   class cli 
   {
      public:
         cli( const client_ptr& c );
         virtual ~cli();

         virtual void print_help();
         virtual void process_command( const std::string& cmd, const std::string& args );

         virtual void list_transactions( uint32_t count = 0 );
         virtual void get_balance( uint32_t min_conf, uint16_t unit = 0 );
         
         std::string get_line( const std::string& prompt = ">>> " );

         void confirm_and_broadcast(signed_transaction& tx);
         void wait();


      protected:
         virtual client_ptr client();
         virtual bool check_unlock();
      private:
         std::unique_ptr<detail::cli_impl> my;
   };

} } // bts::cli
