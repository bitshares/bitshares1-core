#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/net/node.hpp>

#include <bts/rpc_stubs/common_api_rpc_client.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>

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

       void connect_to(const fc::ip::endpoint& remote_endpoint);

       bool login(const std::string& username, const std::string& password);
       virtual fc::rpc::json_connection_ptr get_json_connection() const override;
     private:
       std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
