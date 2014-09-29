#include <bts/blockchain/account_record.hpp>

#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

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
        return !!delegate_info;
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

    share_type account_record::delegate_pay_rate()const
    {
        if( is_delegate() ) return delegate_info->pay_rate;
        return -1;
    }

}} // bts::blockchain
