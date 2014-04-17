#include <bts/dns/dns_db.hpp>
#include <bts/dns/util.hpp>

namespace bts { namespace dns {

dns_db::dns_db()
{
    set_transaction_validator(std::make_shared<dns_transaction_validator>(this));
}

dns_db::~dns_db()
{
    close();
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
            if (!is_domain_output(output))
                continue;

            set_dns_ref(to_domain_output(output).name, output_reference(tx.id(), i));
        }
    }
}

void dns_db::set_dns_ref(const std::string& key, const bts::blockchain::output_reference& ref)
{
    _dns2ref.store(key, ref);
}

bts::blockchain::output_reference dns_db::get_dns_ref(const std::string& key)
{ try {
    return _dns2ref.fetch(key);
} FC_RETHROW_EXCEPTIONS(warn, "Could not fetch dns key=${key}", ("key", key)) }

bool dns_db::has_dns_ref(const std::string& key)
{
    return _dns2ref.find(key).valid();
}

} } // bts::dns
