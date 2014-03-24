#include <bts/dns/dns_db.hpp>

namespace bts { namespace dns {

    namespace ldb = leveldb;
    namespace detail
    {
        class dns_db_impl
        {
            public:
                dns_db_impl(){}
                bts::db::level_map<std::string, dns_record>  name2record;
            
        };
        
    }
    dns_db::dns_db()
    :my( new detail::dns_db_impl() )
    {
        set_transaction_validator( std::make_shared<dns_transaction_validator>(this) );
    }

    dns_db::~dns_db()
    {
    }

    void dns_db::open( const fc::path& dir, bool create )
    {
        try {
            chain_database::open( dir, create );
            my->name2record.open( dir / "name2record", create );
        } FC_RETHROW_EXCEPTIONS( warn, "Error loading domain database ${dir}", ("dir", dir)("create", create) );
    }

    void dns_db::close() 
    {
        my->name2record.close();
    }

    dns_record dns_db::get_dns_record( const std::string& name )
    { try {
            return my->name2record.fetch(name);
    } FC_RETHROW_EXCEPTIONS( warn, "name: ${name}", ("name", name) ) }

    void dns_db::store( const trx_block& blk, const bts::blockchain::signed_transactions& deterministic_txs )
    {
        for (auto i = 0u; i < blk.trxs.size(); i++)
        {
            for (auto out : blk.trxs[i].outputs)
            {
                if (out.claim_func == 20)
                {
                    dns_record record;
                    auto dns_out = out.as<claim_domain_output>();
                    record.owner = dns_out.owner;
                    record.value = dns_out.value;
                    record.last_price = out.amount; // TODO is this right? should
                    record.last_update_ref = output_reference( blk.trxs[i].id(), i );

                    store_dns_record( dns_out.name, record );
                }
            }
        }
        chain_database::store( blk, deterministic_txs );
    }

    bool dns_db::has_dns_record( const std::string& name ) 
    {
        return my->name2record.find(name).valid();
    }

    void dns_db::store_dns_record( const std::string& name, const dns_record& record)
    {
        my->name2record.store(name, record); 
    }

}} // bts::dns
