#pragma once

#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

    struct unique_transaction_key
    {
        time_point_sec  expiration;
        digest_type     digest;

        unique_transaction_key( const transaction& t, const digest_type& chain_id )
            : expiration( t.expiration ), digest( t.digest( chain_id ) ) {}

        friend bool operator == ( const unique_transaction_key& a, const unique_transaction_key& b )
        {
            return std::tie( a.expiration, a.digest ) == std::tie( b.expiration, b.digest );
        }

        friend bool operator < ( const unique_transaction_key& a, const unique_transaction_key& b )
        {
            return std::tie( a.expiration, a.digest ) < std::tie( b.expiration, b.digest );
        }
    };

    struct transaction_record;
    typedef optional<transaction_record> otransaction_record;

    class chain_interface;
    struct transaction_record : public transaction_evaluation_state
    {
       transaction_record(){}

       transaction_record( const transaction_location& loc, const transaction_evaluation_state& s )
       :transaction_evaluation_state(s),chain_location(loc) {}

       transaction_location chain_location;

       void sanity_check( const chain_interface& )const;
       static otransaction_record lookup( const chain_interface& db, const transaction_id_type& );
       static void store( chain_interface& db, const transaction_id_type&, const transaction_record& );
       static void remove( chain_interface& db, const transaction_id_type& );
    };

    class transaction_db_interface
    {
       friend struct transaction_record;

       virtual otransaction_record transaction_lookup_by_id( const transaction_id_type& )const = 0;

       virtual void transaction_insert_into_id_map( const transaction_id_type&, const transaction_record& ) = 0;
       virtual void transaction_insert_into_unique_set( const transaction& ) = 0;

       virtual void transaction_erase_from_id_map( const transaction_id_type& ) = 0;
       virtual void transaction_erase_from_unique_set( const transaction& ) = 0;
    };

} } // bts::blockchain

FC_REFLECT_DERIVED( bts::blockchain::transaction_record,
        (bts::blockchain::transaction_evaluation_state),
        (chain_location)
        )
