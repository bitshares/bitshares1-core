#include <bts/blockchain/output_factory.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_outputs.hpp>

namespace bts { namespace dns {

dns_db::dns_db()
{
    output_factory::instance().register_output<claim_dns_output>();
}

dns_db::~dns_db()
{
    _dns2ref.close();
}

void dns_db::open(const fc::path& dir, bool create)
{ try {
    chain_database::open(dir, create);
    _dns2ref.open(dir / "dns2ref", create);
} FC_RETHROW_EXCEPTIONS(warn, "Error opening DNS database in dir=${dir} with create=${create}", ("dir", dir) ("create", create)) }

void dns_db::close()
{
    _dns2ref.close();
    chain_database::close();
}

void dns_db::store(const trx_block& blk, const signed_transactions& deterministic_trxs,
                   const block_evaluation_state_ptr& state)
{
    chain_database::store(blk, deterministic_trxs, state);

    for (auto i = 0u; i < blk.trxs.size(); i++)
    {
        auto tx = blk.trxs[i];

        for (auto output : tx.outputs)
        {
            if (!is_dns_output(output))
                continue;

            set_dns_ref(to_dns_output(output).key, output_reference(tx.id(), i));
        }
    }
}

void dns_db::set_dns_ref(const std::string& key, const output_reference& ref)
{
    _dns2ref.store(key, ref);
}

output_reference dns_db::get_dns_ref(const std::string& key)
{ try {
    return _dns2ref.fetch(key);
} FC_RETHROW_EXCEPTIONS(warn, "Could not fetch DNS key=${key}", ("key", key)) }

bool dns_db::has_dns_ref(const std::string& key)
{
    return _dns2ref.find(key).valid();
}

std::map<std::string, output_reference> dns_db::filter(bool (*f)(const std::string&, const output_reference&, dns_db& db))
{
    FC_ASSERT(f != nullptr, "null filter function");
    std::map<std::string, output_reference> map;

    bts::db::level_map<std::string, output_reference>::iterator iter = _dns2ref.begin();
    if (!iter.valid()) return map;

    std::string last;
    _dns2ref.last(last);

    while (true)
    {
        if (f(iter.key(), iter.value(), *this))
            map[iter.key()] = iter.value();

        if (iter.key() == last) break;
        iter++;
    }

    return map;
}

} } // bts::dns
