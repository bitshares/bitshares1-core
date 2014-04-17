#include <iostream>
#include <boost/algorithm/string.hpp>

#include <bts/dns/dns_cli.hpp>

namespace bts { namespace dns {

void dns_cli::process_command(const std::string& cmd, const std::string& args)
{
    std::vector<std::string> argv;
    boost::split(argv, args, boost::is_any_of(" "), boost::token_compress_on);
    //std::stringstream ss(args); TODO: Unnecessary?

    const dns_db_ptr db = std::dynamic_pointer_cast<dns_db>(client()->get_chain());
    const dns_wallet_ptr wallet = std::dynamic_pointer_cast<dns_wallet>(client()->get_wallet());

    if (cmd == "bid_on_domain")
    {
        if (check_unlock())
        {
            FC_ASSERT(argv.size() == 3); // cmd name amount
            std::string name = argv[1];
            asset bid = asset(uint64_t(atoi(argv[2].c_str())));
            signed_transactions tx_pool;

            auto tx = wallet->bid_on_domain(name, bid, tx_pool, *db);

            client()->broadcast_transaction(tx);
        }
    }
    else if (cmd == "auction_domain")
    {
        if (check_unlock())
        {
            FC_ASSERT(argv.size() == 3); // cmd name price
            std::string name = argv[1];
            asset price = asset(uint64_t(atoi(argv[2].c_str())));
            signed_transactions tx_pool;

            auto tx = wallet->auction_domain(name, price, tx_pool, *db);

            client()->broadcast_transaction(tx);
        }
    }
    else if (cmd == "transfer_domain")
    {
        if (check_unlock())
        {
            FC_ASSERT(argv.size() == 3); // cmd name to
            std::string name = argv[1];
            auto to_owner = bts::blockchain::address(argv[2]);
            signed_transactions tx_pool;

            auto tx = wallet->transfer_domain(name, to_owner, tx_pool, *db);

            client()->broadcast_transaction(tx);
        }
    }
    else if (cmd == "update_domain_record")
    {
        if (check_unlock())
        {
            FC_ASSERT(argv.size() == 3); // cmd name path
            std::string name = argv[1];
            asset bid = asset(uint64_t(atoi(argv[2].c_str())));
            signed_transactions tx_pool;

            auto tx = wallet->bid_on_domain(name, bid, tx_pool, *db);

            client()->broadcast_transaction(tx);
        }

        // convert arbitrary json value to string..., this validates that it parses
        // properly.
        //fc::variant val = fc::json::from_string(json_value);
    }
    else if (cmd == "list_active_auctions")
    {
        FC_ASSERT(argv.size() == 1);

        auto active_auctions = get_active_auctions(*db);

        for (auto output : active_auctions)
        {
            auto dns_output = to_domain_output(output);

            std::cout << "[" << output.amount.get_rounded_amount() << "] " << dns_output.name << "\n";
        }
    }
    else if (cmd == "lookup_domain_record")
    {
        FC_ASSERT(argv.size() == 2);
        std::string name = argv[1];

        auto value = lookup_value(name, *db);
        std::string record;
        from_variant(value, record);

        std::cout << record << "\n";
    }
    else
    {
        cli::process_command(cmd, args);
    }
}

void dns_cli::list_transactions(uint32_t count)
{
    cli::list_transactions(count);
}

void dns_cli::get_balance(uint32_t min_conf, uint16_t unit)
{
    cli::get_balance(min_conf, unit);
}

} } //bts::dns
