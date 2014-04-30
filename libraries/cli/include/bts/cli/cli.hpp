#pragma once
#include <bts/client/client.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <fc/variant.hpp>
#include <fc/io/buffered_iostream.hpp>

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

         virtual void list_delegates( uint32_t count = 0 );

         std::string get_line( const std::string& prompt = ">>> ", bool no_echo = false );

         void confirm_and_broadcast(signed_transaction& tx);
         void wait();


        /// hooks to implement custom behavior for interactive command, if the default json-style behavior is undesirable
        virtual fc::variant parse_argument_of_known_type(fc::buffered_istream& argument_stream,
                                                         const bts::rpc::rpc_server::method_data& method_data,
                                                         unsigned parameter_index);
        virtual fc::variants parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream,
                                                                    const std::string& command);
        virtual fc::variants parse_recognized_interactive_command(fc::buffered_istream& argument_stream,
                                                                  const bts::rpc::rpc_server::method_data& method_data);
        virtual fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command);
        virtual fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments);
        virtual void format_and_print_result(const std::string& command, const fc::variant& result);

      private:
         std::unique_ptr<detail::cli_impl> my;
   };

} } // bts::cli
