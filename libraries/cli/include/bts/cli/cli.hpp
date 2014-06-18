#pragma once
#include <bts/client/client.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <fc/variant.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <bts/api/api_metadata.hpp>
#include <boost/optional.hpp>
#include "boost/iostreams/stream.hpp"
#include "boost/iostreams/device/null.hpp"

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
          cli( const client_ptr& client,
               std::istream* command_script = nullptr, 
               std::ostream* output_stream = nullptr );

          virtual ~cli();
          void start();

          void set_input_stream_log(boost::optional<std::ostream&> input_stream_log);
          void set_daemon_mode(bool enable_daemon_mode);
          void display_status_message(const std::string& message);
          void process_commands(std::istream* input_stream);

          //Parse and execute a command line. Returns false if line is a quit command.
          bool execute_command_line(const std::string& line, std::ostream* output = nullptr);
          void confirm_and_broadcast(signed_transaction& tx);
          void wait_till_cli_shutdown();

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
          virtual void format_and_print_result(const std::string& command,
                                               const fc::variants& arguments,
                                               const fc::variant& result);

      private:
          std::unique_ptr<detail::cli_impl> my;
   };
   typedef std::shared_ptr<cli> cli_ptr;

} } // bts::cli
