#pragma once

#include <bts/blockchain/asset.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

enum account_key_type
{
    owner_key    = 0,
    active_key   = 1,
    signing_key  = 2
};

enum account_type
{
    titan_account    = 0,
    public_account   = 1,
    multisig_account = 2
};

struct multisig_meta_info
{
    static const account_type type = multisig_account;

    uint32_t     required = 1;
    set<address> owners;
};

struct account_meta_info
{
    fc::enum_type<fc::unsigned_int,account_type> type = public_account;
    vector<char>                                 data;

    account_meta_info( account_type atype = public_account )
    :type( atype ){}

    template<typename AccountType>
    account_meta_info(  const AccountType& t )
    :type( AccountType::type )
    {
      data = fc::raw::pack( t );
    }

    template<typename AccountType>
    AccountType as()const
    {
      FC_ASSERT( type == AccountType::type, "", ("AccountType",AccountType::type) );
      return fc::raw::unpack<AccountType>(data);
    }
};

struct delegate_stats
{
    share_type                        votes_for = 0;

    uint8_t                           pay_rate = 0;
    map<uint32_t, public_key_type>    signing_key_history;

    uint32_t                          last_block_num_produced = 0;
    optional<secret_hash_type>        next_secret_hash;

    share_type                        pay_balance = 0;
    share_type                        total_paid = 0;
    share_type                        total_burned = 0;

    uint32_t                          blocks_produced = 0;
    uint32_t                          blocks_missed = 0;
};
typedef fc::optional<delegate_stats> odelegate_stats;

struct vote_del
{
    share_type        votes = 0;
    account_id_type   delegate_id = 0;

    vote_del( int64_t v = 0, account_id_type del = 0 )
    :votes(v),delegate_id(del){}

    friend bool operator == ( const vote_del& a, const vote_del& b )
    {
       return std::tie( a.votes, a.delegate_id ) == std::tie( b.votes, b.delegate_id );
    }

    friend bool operator < ( const vote_del& a, const vote_del& b )
    {
       if( a.votes != b.votes ) return a.votes > b.votes; /* Reverse so maps sort in descending order */
       return a.delegate_id < b.delegate_id; /* Lowest id wins in ties */
    }
};

struct account_record;
typedef fc::optional<account_record> oaccount_record;

class chain_interface;
struct account_record
{
    address         owner_address()const { return address( owner_key ); }

    void            set_active_key( const time_point_sec now, const public_key_type& new_key );
    public_key_type active_key()const;
    address         active_address()const;
    bool            is_retracted()const;

    bool            is_delegate()const;
    uint8_t         delegate_pay_rate()const;
    void            adjust_votes_for( const share_type delta );
    share_type      net_votes()const;
    share_type      delegate_pay_balance()const;

    void            set_signing_key( uint32_t block_num, const public_key_type& signing_key );
    public_key_type signing_key()const;
    address         signing_address()const;

    bool            is_titan_account()const { return meta_data.valid() && meta_data->type == titan_account; }

    void            scan_public_keys( const function<void( const public_key_type& )> )const;

    account_id_type                        id = 0;
    std::string                            name;
    fc::variant                            public_data;
    public_key_type                        owner_key;
    map<time_point_sec, public_key_type>   active_key_history;
    fc::time_point_sec                     registration_date;
    fc::time_point_sec                     last_update;
    optional<delegate_stats>               delegate_info;
    optional<account_meta_info>            meta_data;

    void sanity_check( const chain_interface& )const;
    static oaccount_record lookup( const chain_interface&, const account_id_type );
    static oaccount_record lookup( const chain_interface&, const string& );
    static oaccount_record lookup( const chain_interface&, const address& );
    static void store( chain_interface&, const account_id_type, const account_record& );
    static void remove( chain_interface&, const account_id_type );
};

class account_db_interface
{
    friend struct account_record;

    virtual oaccount_record account_lookup_by_id( const account_id_type )const = 0;
    virtual oaccount_record account_lookup_by_name( const string& )const = 0;
    virtual oaccount_record account_lookup_by_address( const address& )const = 0;

    virtual void account_insert_into_id_map( const account_id_type, const account_record& ) = 0;
    virtual void account_insert_into_name_map( const string&, const account_id_type ) = 0;
    virtual void account_insert_into_address_map( const address&, const account_id_type ) = 0;
    virtual void account_insert_into_vote_set( const vote_del& ) = 0;

    virtual void account_erase_from_id_map( const account_id_type ) = 0;
    virtual void account_erase_from_name_map( const string& ) = 0;
    virtual void account_erase_from_address_map( const address& ) = 0;
    virtual void account_erase_from_vote_set( const vote_del& ) = 0;
};

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::account_key_type,
                 (owner_key)
                 (active_key)
                 (signing_key)
                 )
FC_REFLECT_ENUM( bts::blockchain::account_type,
                 (titan_account)
                 (public_account)
                 (multisig_account)
                 )
FC_REFLECT( bts::blockchain::multisig_meta_info,
            (required)
            (owners)
            )
FC_REFLECT( bts::blockchain::account_meta_info,
            (type)
            (data)
            )
FC_REFLECT( bts::blockchain::delegate_stats,
            (votes_for)
            (pay_rate)
            (signing_key_history)
            (last_block_num_produced)
            (next_secret_hash)
            (pay_balance)
            (total_paid)
            (total_burned)
            (blocks_produced)
            (blocks_missed)
            )
FC_REFLECT( bts::blockchain::vote_del,
            (votes)
            (delegate_id)
            )
FC_REFLECT( bts::blockchain::account_record,
            (id)
            (name)
            (public_data)
            (owner_key)
            (active_key_history)
            (registration_date)
            (last_update)
            (delegate_info)
            (meta_data)
            )
