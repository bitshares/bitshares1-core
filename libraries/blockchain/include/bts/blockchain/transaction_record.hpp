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

    class chain_interface;
    struct transaction_db_interface;
    struct transaction_record : public transaction_evaluation_state
    {
       transaction_record(){}

       transaction_record( const transaction_location& loc, const transaction_evaluation_state& s )
       :transaction_evaluation_state(s),chain_location(loc) {}

       transaction_location chain_location;

       static const transaction_db_interface& db_interface( const chain_interface& );
    };
    typedef optional<transaction_record> otransaction_record;

    struct transaction_db_interface
    {
        std::function<otransaction_record( const transaction_id_type& )>                lookup_by_id;

        std::function<void( const transaction_id_type&, const transaction_record& )>    insert_into_id_map;
        std::function<void( const transaction& )>                                       insert_into_unique_set;

        std::function<void( const transaction_id_type& )>                               erase_from_id_map;
        std::function<void( const transaction& )>                                       erase_from_unique_set;

        otransaction_record lookup( const transaction_id_type& )const;
        void store( const transaction_id_type&, const transaction_record& )const;
        void remove( const transaction_id_type& )const;
    };

} } // bts::blockchain

FC_REFLECT_DERIVED( bts::blockchain::transaction_record,
        (bts::blockchain::transaction_evaluation_state),
        (chain_location)
        )
