#include <bts/dns/dns_util.hpp>

namespace bts { namespace dns {

    // Check if name exists in current block state or db
    bool name_exists(const std::string &name, const signed_transactions &outstanding_transactions, dns_db &db)
    {
        for (auto &tx : outstanding_transactions)
        {
            for (auto &out : tx.outputs)
            {
                if (out.claim_func != claim_domain)
                    continue;

                if (out.as<claim_domain_output>().name == name)
                    return true;
            }
        }

        return db.has_dns_record(name);
    }

    /* Check if name is available for bid: new, in auction, or expired */
    bool can_bid_on_name(const std::string &name, const signed_transactions &outstanding_transactions, dns_db &db,
                         bool &name_exists, trx_output &prev_output)
    {
        FC_ASSERT(name.size() <= BTS_DNS_MAX_NAME_LEN, "Max name length exceeded: ${len}", ("len", name.size()));

        // TODO: check outstanding transactions for buys or sells

        name_exists = db.has_dns_record(name);
        if (!name_exists)
            return true;

        auto record = db.get_dns_record(name);
        auto tx_ref = record.last_update_ref;
        prev_output = db.fetch_output(tx_ref);

        auto tx_ref_id = tx_ref.trx_hash;
        auto block_num = db.fetch_trx_num(tx_ref_id).block_num;
        auto age = db.head_block_num() - block_num;

        return !((age >= DNS_AUCTION_DURATION_BLOCKS) && (age < DNS_EXPIRE_DURATION_BLOCKS));
    }

    bool is_valid_bid(const trx_output &output, const signed_transactions &outstanding_transactions, dns_db &db,
                      bool &name_exists, trx_output &prev_output)
    {
        FC_ASSERT(output.claim_func == claim_domain, "Invalid output type");
        auto dns_output = output.as<claim_domain_output>();

        if (dns_output.state == claim_domain_output::not_in_auction)
            return false;

        FC_ASSERT(dns_output.state == claim_domain_output::possibly_in_auction, "Invalid output state");

        return can_bid_on_name(dns_output.name, outstanding_transactions, db, name_exists, prev_output);
    }

}} // bts::dns
