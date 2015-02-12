#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

struct slate_record;
typedef optional<slate_record> oslate_record;

class chain_interface;
struct slate_record
{
    set<account_id_type> slate;
    vector<account_id_type> duplicate_slate;

    slate_id_type id()const;

    static slate_id_type id_v1( const vector<account_id_type>& slate )
    {
        if( slate.empty() ) return 0;
        return fc::sha256::hash( slate )._hash[ 0 ];
    }

    void sanity_check( const chain_interface& )const;
    static oslate_record lookup( const chain_interface&, const slate_id_type );
    static void store( chain_interface&, const slate_id_type, const slate_record& );
    static void remove( chain_interface&, const slate_id_type );
};

class slate_db_interface
{
    friend struct slate_record;

    virtual oslate_record slate_lookup_by_id( const slate_id_type )const = 0;
    virtual void slate_insert_into_id_map( const slate_id_type, const slate_record& ) = 0;
    virtual void slate_erase_from_id_map( const slate_id_type ) = 0;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::slate_record, (slate)(duplicate_slate) )
