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

    // Check if name exists in current block state or db
    bool output_name_exists(const claim_domain_output &output,
            const signed_transactions &outstanding_transactions, dns_db &db)
    {
        auto name = output.name;

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

    bool name_is_in_auction(const std::string &name, const signed_transactions &outstanding_transactions,
                            dns_db &db, trx_output &prev_output)
    {
        // TODO: check current transactions for buy or sell actions
        //
        //

        if (!db.has_dns_record(name))
            return false;

        auto record = db.get_dns_record(name);
        auto tx_ref = record.last_update_ref;
        prev_output = db.fetch_output(tx_ref);

        auto tx_ref_id = tx_ref.trx_hash;
        auto block_num = db.fetch_trx_num(tx_ref_id).block_num;
        auto age = db.head_block_num() - block_num;

        return !((age >= DNS_AUCTION_DURATION_BLOCKS) && (age < DNS_EXPIRE_DURATION_BLOCKS));
    }

    bool output_is_in_auction(const claim_domain_output &output,
            const signed_transactions &outstanding_transactions, dns_db &db)
    {
        if (output.state == claim_domain_output::not_in_auction)
            return false;

        FC_ASSERT(output.state == claim_domain_output::possibly_in_auction, "Invalid output state");

        // FIXME
        //return name_is_in_auction(output.name, outstanding_transactions, db);
        return false;
    }

}} // bts::dns
