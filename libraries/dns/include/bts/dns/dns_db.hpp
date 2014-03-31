#pragma once

#include <fc/reflect/variant.hpp>

#include <bts/db/level_map.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/chain_database.hpp>

#include <bts/dns/dns_transaction_validator.hpp>

namespace bts { namespace dns {

namespace detail  { class dns_db_impl; }

struct dns_record
{
    bts::blockchain::address           owner;
    std::vector<char>                  value;
    bts::blockchain::asset             last_price; // TODO delete? this isn't used since it's on the tx out
    bts::blockchain::output_reference  last_update_ref;
};

class dns_db : public bts::blockchain::chain_database
{
    public:
        dns_db();
        ~dns_db();
    
        void             open( const fc::path& dir, bool create );
        void             close();
        dns_record       get_dns_record( const std::string& name );
        bool             has_dns_record( const std::string& name );
        void             store_dns_record( const std::string& name, const dns_record& record);
        void             store( const trx_block& blk, const signed_transactions& deterministic_txs );

    private:
         std::unique_ptr<detail::dns_db_impl> my;
};

}} // bts::dns

FC_REFLECT( bts::dns::dns_record, (owner)(value)(last_price)(last_update_ref) );
