#include <bts/blockchain/account_record.hpp>

#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

    //account_type multisig_meta_info::type = multisig_account;

    bool account_record::is_null()const
    {
        return owner_key == public_key_type();
    }

    account_record account_record::make_null()const
    {
        account_record cpy(*this);
        cpy.owner_key = public_key_type();
        return cpy;
    }

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

    void account_record::adjust_votes_for( share_type delta )
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
        return address(active_key());
    }

    void account_record::set_active_key( const time_point_sec& now, const public_key_type& new_key )
    { try {
        FC_ASSERT( now != fc::time_point_sec() );
        active_key_history[now] = new_key;
    } FC_CAPTURE_AND_RETHROW( (now)(new_key) ) }

    public_key_type account_record::active_key()const
    {
        if( active_key_history.size() )
            return active_key_history.rbegin()->second;

        return public_key_type();
    }

    uint8_t account_record::delegate_pay_rate()const
    {
        if( is_delegate() ) return delegate_info->pay_rate;
        return -1;
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

}} // bts::blockchain
