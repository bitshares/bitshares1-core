#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

    share_type account_record::delegate_pay_balance()const
    {
        FC_ASSERT( is_delegate() );
        return delegate_info->pay_balance;
    }

    bool account_record::is_delegate()const
    {
        return delegate_info.valid();
    }

    int64_t account_record::net_votes()const
    {
        FC_ASSERT( is_delegate() );
        return delegate_info->votes_for;
    }

    void account_record::adjust_votes_for( const share_type delta )
    {
        FC_ASSERT( is_delegate() );
        delegate_info->votes_for += delta;
    }

    bool account_record::is_retracted()const
    {
        return active_key() == public_key_type();
    }

    address account_record::active_address()const
    {
        return address( active_key() );
    }

    void account_record::set_active_key( const time_point_sec now, const public_key_type& new_key )
    { try {
        FC_ASSERT( now != fc::time_point_sec() );
        active_key_history[ now ] = new_key;
    } FC_CAPTURE_AND_RETHROW( (now)(new_key) ) }

    public_key_type account_record::active_key()const
    {
        if( active_key_history.empty() )
            return public_key_type();

        return active_key_history.rbegin()->second;
    }

    uint8_t account_record::delegate_pay_rate()const
    {
        if( !is_delegate() )
            return -1;

        return delegate_info->pay_rate;
    }

    void account_record::set_signing_key( uint32_t block_num, const public_key_type& signing_key )
    {
        FC_ASSERT( is_delegate() );
        delegate_info->signing_key_history[ block_num ] = signing_key;
    }

    public_key_type account_record::signing_key()const
    {
        FC_ASSERT( is_delegate() );
        FC_ASSERT( !delegate_info->signing_key_history.empty() );
        return delegate_info->signing_key_history.crbegin()->second;
    }

    address account_record::signing_address()const
    {
        return address( signing_key() );
    }

    public_key_type burn_record_value::signer_key()const
    {
       FC_ASSERT( signer.valid() );
       fc::sha256 digest;
       if( message.size() )
          digest = fc::sha256::hash( message.c_str(), message.size() );
       return fc::ecc::public_key( *signer, digest );
    }

    const account_db_interface& account_record::db_interface( const chain_interface& db )
    { try {
        return db._account_db_interface;
    } FC_CAPTURE_AND_RETHROW() }

    oaccount_record account_db_interface::lookup( const account_id_type id )const
    { try {
        return lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    oaccount_record account_db_interface::lookup( const string& name )const
    { try {
        return lookup_by_name( name );
    } FC_CAPTURE_AND_RETHROW( (name) ) }

    oaccount_record account_db_interface::lookup( const address& addr )const
    { try {
        return lookup_by_address( addr );
    } FC_CAPTURE_AND_RETHROW( (addr) ) }

    void account_db_interface::store( const account_id_type id, const account_record& record )const
    { try {
        const oaccount_record prev_record = lookup( id );
        if( prev_record.valid() )
        {
            if( prev_record->name != record.name )
                erase_from_name_map( prev_record->name );
            if( prev_record->owner_address() != record.owner_address() )
                erase_from_address_map( prev_record->owner_address() );
            if( !prev_record->is_retracted() )
            {
                if( record.is_retracted() || prev_record->active_address() != record.active_address() )
                    erase_from_address_map( prev_record->active_address() );
                if( prev_record->is_delegate() )
                {
                    if( record.is_retracted() || !record.is_delegate() || prev_record->signing_address() != record.signing_address() )
                        erase_from_address_map( prev_record->signing_address() );
                    if( record.is_retracted() || !record.is_delegate() || prev_record->net_votes() != record.net_votes() )
                        erase_from_vote_set( vote_del( prev_record->net_votes(), prev_record->id ) );
                }
            }
        }

        insert_into_id_map( id, record );
        insert_into_name_map( record.name, id );
        insert_into_address_map( record.owner_address(), id );
        if( !record.is_retracted() )
        {
            insert_into_address_map( record.active_address(), id );
            if( record.is_delegate() )
            {
                insert_into_address_map( record.signing_address(), id );
                insert_into_vote_set( vote_del( record.net_votes(), id ) );
            }
        }
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void account_db_interface::remove( const account_id_type id )const
    { try {
        const oaccount_record prev_record = lookup( id );
        if( prev_record.valid() )
        {
            erase_from_id_map( id );
            erase_from_name_map( prev_record->name );
            erase_from_address_map( prev_record->owner_address() );
            if( !prev_record->is_retracted() )
            {
                erase_from_address_map( prev_record->active_address() );
                if( prev_record->is_delegate() )
                {
                    erase_from_address_map( prev_record->signing_address() );
                    erase_from_vote_set( vote_del( prev_record->net_votes(), prev_record->id ) );
                }
            }
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
