#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/db/level_map.hpp>
#include <bts/dns/dns_outputs.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

class dns_db : public bts::blockchain::chain_database
{
    public:
        dns_db();
        ~dns_db();

        virtual void open(const fc::path& dir, bool create = true);
        virtual void close();
        virtual void store(const trx_block& blk, const signed_transactions& deterministic_trxs,
                           const block_evaluation_state_ptr& state);

        // TODO: Add delete operation
        void                              set_dns_ref(const std::string& key, const output_reference& ref);
        output_reference get_dns_ref(const std::string& key);
        bool                              has_dns_ref(const std::string& key);

        std::map<std::string, output_reference>
            filter(bool (*f)(const std::string&, const output_reference&, dns_db& db));

        trx_output get_key_output(const std::string &key);

        // TODO: move to base class
        uint32_t get_tx_age(const output_reference &tx_ref);

    private:
        bts::db::level_map<std::string, output_reference>  _dns2ref;
};

typedef std::shared_ptr<dns_db> dns_db_ptr;

} } // bts::dns
