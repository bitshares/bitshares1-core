#pragma once

#include <bts/blockchain/asset.hpp>

namespace bts { namespace blockchain {

struct burn_index
{
    account_id_type     account_id;
    transaction_id_type transaction_id;

    friend bool operator < ( const burn_index& a, const burn_index& b )
    {
        return std::tie( a.account_id, a.transaction_id ) < std::tie( b.account_id, b.transaction_id );
    }

    friend bool operator == ( const burn_index& a, const burn_index& b )
    {
        return std::tie( a.account_id, a.transaction_id ) == std::tie( b.account_id, b.transaction_id );
    }
};

struct burn_record;
typedef fc::optional<burn_record> oburn_record;

class chain_interface;
struct burn_record
{
    burn_index                  index;
    asset                       amount;
    string                      message;
    optional<signature_type>    signer;

    public_key_type signer_key()const;

    void sanity_check( const chain_interface& )const;
    static oburn_record lookup( const chain_interface&, const burn_index& );
    static void store( chain_interface&, const burn_index&, const burn_record& );
    static void remove( chain_interface&, const burn_index& );
};

class burn_db_interface
{
    friend struct burn_record;

    virtual oburn_record burn_lookup_by_index( const burn_index& )const = 0;
    virtual void burn_insert_into_index_map( const burn_index&, const burn_record& ) = 0;
    virtual void burn_erase_from_index_map( const burn_index& ) = 0;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::burn_index, (account_id)(transaction_id) )
FC_REFLECT( bts::blockchain::burn_record, (index)(amount)(message)(signer) )
