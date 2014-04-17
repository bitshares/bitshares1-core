#pragma once

#include <fc/reflect/variant.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/db/level_map.hpp>

#include <bts/dns/dns_transaction_validator.hpp>

namespace bts { namespace dns {

class dns_db : public bts::blockchain::chain_database
{
    public:
        dns_db();
        ~dns_db();

        virtual void open(const fc::path& dir, bool create = true);
        virtual void close();
        virtual void store(const trx_block& blk, const signed_transactions& deterministic_trxs,
                           const block_evaluation_state_ptr& state);

        void                              set_dns_ref(const std::string& key, const bts::blockchain::output_reference& ref);
        bts::blockchain::output_reference get_dns_ref(const std::string& key);
        bool                              has_dns_ref(const std::string& key);

    private:
        bts::db::level_map<std::string, bts::blockchain::output_reference> _dns2ref;
};

typedef std::shared_ptr<dns_db> dns_db_ptr;

} } // bts::dns
