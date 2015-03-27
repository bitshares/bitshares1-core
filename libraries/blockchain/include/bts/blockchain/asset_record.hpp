#pragma once

#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

struct asset_record;
typedef fc::optional<asset_record> oasset_record;

class chain_interface;
struct transaction_evaluation_state;
struct asset_record
{
    enum
    {
        god_issuer_id     =  0,
        null_issuer_id    = -1,
        market_issuer_id  = -2
    };

    enum flag_enum
    {
        dynamic_max_supply      = 1 << 0, // Max supply can be changed even if current supply is greater than zero
        dynamic_fees            = 1 << 1, // Transaction and market fees can be changed even if current supply is greater than zero
        halted_markets          = 1 << 2, // Markets cannot execute
        halted_withdrawals      = 1 << 3, // Balances cannot be withdrawn
        retractable_balances    = 1 << 4, // Authority can withdraw from any balance or market order
        restricted_accounts     = 1 << 5  // Only whitelisted account active and owner keys can receive deposits and market payouts
    };

    bool is_market_issued()const      { return issuer_id == market_issuer_id; };
    bool is_user_issued()const        { return issuer_id > god_issuer_id; };

    bool authority_has_flag_permission( const flag_enum permission )const
    { return is_user_issued() && (authority_flag_permissions & permission); }

    bool flag_is_active( const flag_enum flag )const
    { return authority_has_flag_permission( flag ) && (active_flags & flag); }

    bool address_is_approved( const chain_interface& db, const address& addr )const;
    bool authority_is_retracting( const transaction_evaluation_state& eval_state )const;

    static uint64_t share_string_to_precision_unsafe( const string& share_string_with_trailing_decimals );
    static share_type share_string_to_satoshi( const string& share_string, const uint64_t precision );

    asset asset_from_string( const string& amount )const;
    string amount_to_string( share_type amount, bool append_symbol = true )const;

    asset_id_type           id;
    string                  symbol;

    account_id_type         issuer_id = null_issuer_id;

    multisig_meta_info      authority;
    uint32_t                authority_flag_permissions = uint32_t( -1 );
    uint32_t                active_flags = 0;
    set<account_id_type>    whitelist;

    string                  name;
    string                  description;
    variant                 public_data;

    uint64_t                precision = 0; // Power of ten representing the max divisibility
    share_type              max_supply = 0;

    share_type              withdrawal_fee = 0;
    uint16_t                market_fee_rate = 0;

    share_type              collected_fees = 0;
    share_type              current_supply = 0;

    time_point_sec          registration_date;
    time_point_sec          last_update;

    void sanity_check( const chain_interface& )const;
    static oasset_record lookup( const chain_interface&, const asset_id_type );
    static oasset_record lookup( const chain_interface&, const string& );
    static void store( chain_interface&, const asset_id_type, const asset_record& );
    static void remove( chain_interface&, const asset_id_type );
};

class asset_db_interface
{
    friend struct asset_record;

    virtual oasset_record asset_lookup_by_id( const asset_id_type )const = 0;
    virtual oasset_record asset_lookup_by_symbol( const string& )const = 0;

    virtual void asset_insert_into_id_map( const asset_id_type, const asset_record& ) = 0;
    virtual void asset_insert_into_symbol_map( const string&, const asset_id_type ) = 0;

    virtual void asset_erase_from_id_map( const asset_id_type ) = 0;
    virtual void asset_erase_from_symbol_map( const string& ) = 0;
};

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::asset_record::flag_enum,
        (dynamic_max_supply)
        (dynamic_fees)
        (halted_markets)
        (halted_withdrawals)
        (retractable_balances)
        (restricted_accounts)
        )
FC_REFLECT( bts::blockchain::asset_record,
        (id)
        (symbol)
        (issuer_id)
        (authority)
        (authority_flag_permissions)
        (active_flags)
        (whitelist)
        (name)
        (description)
        (public_data)
        (precision)
        (max_supply)
        (withdrawal_fee)
        (market_fee_rate)
        (collected_fees)
        (current_supply)
        (registration_date)
        (last_update)
        )
