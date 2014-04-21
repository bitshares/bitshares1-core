#pragma once
#include <bts/client/client.hpp>

namespace bts { namespace rpc {
  class rpc_server;
  typedef std::shared_ptr<rpc_server> rpc_server_ptr;
} }

namespace bts { namespace cli {
   
   using namespace client;

   namespace detail {  class cli_impl; }

   class cli 
   {
      public:
         cli( const client_ptr& client, const bts::rpc::rpc_server_ptr& rpc_server );
         virtual ~cli();

         virtual void print_help();
         //virtual void process_command( const std::string& cmd, const std::string& args );

         virtual void list_transactions( uint32_t count = 0 );
         
         std::string get_line( const std::string& prompt = ">>> " );

         void confirm_and_broadcast(signed_transaction& tx);
         void wait();


        /// hooks to implement custom behavior for interactive command, if the default json-style behavior is undesirable
        virtual void parse_interactive_command(const std::string& line_to_parse, std::string& command, fc::variants& arguments);
        virtual fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments);
        virtual void format_and_print_result(const std::string& command, const fc::variant& result);
      protected:
         virtual client_ptr client();
         virtual bool check_unlock();
      private:
         std::unique_ptr<detail::cli_impl> my;
   };

} } // bts::cli
