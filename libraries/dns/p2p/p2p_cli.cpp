#include <bts/dns/dns_outputs.hpp>
#include <bts/dns/p2p/p2p_cli.hpp>
#include <iostream>

namespace bts { namespace dns { namespace p2p {

p2p_cli::p2p_cli(const client_ptr& client, const p2p_rpc_server_ptr& rpc_server) : bts::cli::cli(client, rpc_server)
{
}

p2p_cli::~p2p_cli()
{
}

void p2p_cli::format_and_print_result(const std::string& command, const fc::variant& result)
{
    if (command == "list_active_domain_auctions")
    {
        auto auctions = result.as<std::vector<std::pair<asset, claim_dns_output>>>();

        for (auto auction : auctions)
            std::cout << "[" << auction.first.amount << "] " << auction.second.key << "\n";
    }
    else
    {
        bts::cli::cli::format_and_print_result(command, result);
    }
}

} } } // bts::dns::p2p
