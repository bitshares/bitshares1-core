#include <iostream>
#include <boost/algorithm/string.hpp>

#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/dns_cli.hpp>

namespace bts { namespace dns {

void dns_cli::format_and_print_result(const std::string& command, const fc::variant& result)
{
  if (command == "list_active_auctions")
  {
    std::vector<std::pair<bts::blockchain::asset, claim_domain_output> > active_auctions = result.as<std::vector<std::pair<bts::blockchain::asset, claim_domain_output> > >();
    for (const std::pair<bts::blockchain::asset, claim_domain_output>& active_auction : active_auctions)
    {
      std::cout << "[" << active_auction.first.get_rounded_amount() << "] " << active_auction.second.name << "\n";
    }
  }
  else
    bts::cli::cli::format_and_print_result(command, result);
}

} } //bts::dns
