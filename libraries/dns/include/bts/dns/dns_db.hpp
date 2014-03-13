#pragma once
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace dns {

namespace detail  { class dns_db_impl; }

struct dns_record
{
    bts::blockchain::address           owner;
    std::vector<char>                  value;
    bts::blockchain::asset             last_price;
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

    private:
         std::unique_ptr<detail::dns_db_impl> my;

};


}} // bts::dns


FC_REFLECT( bts::dns::dns_record, (owner)(value)(last_price)(last_update_ref) );
