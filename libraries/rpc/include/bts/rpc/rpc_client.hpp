#pragma once

#include <bts/net/node.hpp>
#include <bts/rpc_stubs/common_api_rpc_client.hpp>
#include <bts/wallet/wallet.hpp>

#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>

#include <memory>

namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }

  /**
  *  @class rpc_client
  *  @brief provides a C++ interface to a remote BTS client over JSON-RPC
  */
  class rpc_client : public bts::rpc_stubs::common_api_rpc_client
  {
     public:
       rpc_client();
       virtual ~rpc_client();

       void connect_to(const fc::ip::endpoint& remote_endpoint,
                       const blockchain::public_key_type& remote_public_key = blockchain::public_key_type());

       bool login(const std::string& username, const std::string& password);
       virtual fc::rpc::json_connection_ptr get_json_connection() const override;
       void reset_json_connection();
     private:
       std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
