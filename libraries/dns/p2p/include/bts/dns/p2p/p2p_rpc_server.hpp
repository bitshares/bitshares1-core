#pragma once
#include <bts/rpc/rpc_server.hpp>

namespace bts { namespace dns {
  
  namespace detail
  {
    class dns_rpc_server_impl;
  }
  
  class dns_rpc_server : public bts::rpc::rpc_server
  {
  public:
    dns_rpc_server();
    ~dns_rpc_server();
  private:
    std::unique_ptr<detail::dns_rpc_server_impl> my;
  };
  typedef std::shared_ptr<dns_rpc_server> dns_rpc_server_ptr;
} } // bts::dns
