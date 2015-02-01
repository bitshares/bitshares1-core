#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

class chain_interface;
struct slate_db_interface;
struct slate_record
{
    set<account_id_type> slate;

    slate_id_type id()const;

    static const slate_db_interface& db_interface( const chain_interface& );
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

FC_REFLECT( bts::blockchain::slate_record, (slate) )
