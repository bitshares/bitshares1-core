#pragma once

#include <bts/cli/cli.hpp>
#include <bts/dns/p2p/p2p_rpc_server.hpp>

namespace bts { namespace dns { namespace p2p {

    using namespace client;

    class p2p_cli : public bts::cli::cli
    {
        public:
            p2p_cli(const client_ptr& client, const p2p_rpc_server_ptr& rpc_server);
            virtual ~p2p_cli() override;

            virtual void format_and_print_result(const std::string& command, const fc::variant& result) override;
    };

} } } // bts::dns::p2p
