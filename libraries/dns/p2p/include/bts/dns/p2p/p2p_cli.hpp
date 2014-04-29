#pragma once

#include <sstream>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>

#include <bts/cli/cli.hpp>

namespace bts { namespace dns {

  using namespace client;

  class dns_cli : public bts::cli::cli
  {
  public:
    dns_cli (const client_ptr &c, const bts::rpc::rpc_server_ptr& rpc_server) : cli(c, rpc_server)
    {
    }
    virtual void format_and_print_result(const std::string& command, const fc::variant& result) override;
      
  };

} } // bts::dns
