#pragma once
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace dns {

namespace detail  { class me_db_impl; }

class me_db : public bts::blockchain::chain_database
{
    public:
        me_db();
        ~me_db();
    
        void             open( const fc::path& dir, bool create );
        void             close();

    private:
         std::unique_ptr<detail::me_db_impl> my;
};


}} // bts::dns


FC_REFLECT( bts::dns::dns_record, (owner)(value)(last_price)(last_update_ref) );
