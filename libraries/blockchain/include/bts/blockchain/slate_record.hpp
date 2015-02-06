#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

class chain_interface;
struct slate_db_interface;
struct slate_record
{
    set<account_id_type> slate;
    vector<account_id_type> duplicate_slate;

    slate_id_type id()const;

    static const slate_db_interface& db_interface( const chain_interface& );

    static slate_id_type id_v1( const vector<account_id_type>& slate )
    {
        if( slate.empty() ) return 0;
        fc::sha256::encoder enc;
        fc::raw::pack( enc, slate );
        return enc.result()._hash[ 0 ];
    }
};
typedef optional<slate_record> oslate_record;

struct slate_db_interface
{
    std::function<oslate_record( const slate_id_type )>             lookup_by_id;
    std::function<void( const slate_id_type, const slate_record& )> insert_into_id_map;
    std::function<void( const slate_id_type )>                      erase_from_id_map;

    oslate_record lookup( const slate_id_type )const;
    void store( const slate_id_type, const slate_record& )const;
    void remove( const slate_id_type )const;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::slate_record, (slate)(duplicate_slate) )
