#pragma once
#include <bts/client/client.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <fc/variant.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <bts/api/api_metadata.hpp>
#include <boost/optional.hpp>

namespace bts { namespace rpc {
  class rpc_server;
  typedef std::shared_ptr<rpc_server> rpc_server_ptr;
} }

#define CLI_PROMPT_SUFFIX ">>> "

namespace bts { namespace cli {

   using namespace bts::client;
   using namespace bts::rpc;
   using namespace bts::wallet;

   namespace detail {  class cli_impl; }

   class cli
   {
      public:
          cli( const client_ptr& client, std::istream& input_stream, std::ostream& output_stream);
          virtual ~cli();
          void set_input_log_stream(boost::optional<std::ostream&> input_log_stream);

          void process_commands();

          //Parse and execute a command line. Returns false if line is a quit command.
          bool execute_command_line(const std::string& line);
          void confirm_and_broadcast(signed_transaction& tx);
          void wait();
          void quit();

          // hooks to implement custom behavior for interactive command, if the default json-style behavior is undesirable
          virtual fc::variant parse_argument_of_known_type(fc::buffered_istream& argument_stream,
                                                         const bts::api::method_data& method_data,
                                                         unsigned parameter_index);
          virtual fc::variants parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream,
                                                                    const std::string& command);
          virtual fc::variants parse_recognized_interactive_command(fc::buffered_istream& argument_stream,
                                                                  const bts::api::method_data& method_data);
          virtual fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command);
          virtual fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments);
          virtual void format_and_print_result(const std::string& command, const fc::variant& result);

      private:
          std::unique_ptr<detail::cli_impl> my;
   };
   typedef std::shared_ptr<cli> cli_ptr;

} } // bts::cli
