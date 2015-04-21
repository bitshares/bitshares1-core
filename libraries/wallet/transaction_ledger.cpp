// TODO: Everything in this file needs to be rewritten in transaction_ledger_experimental.cpp
// When GitHub issue #845 is done, then this file can be deleted

#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/time.hpp>

#include <sstream>

using namespace bts::wallet;
using namespace bts::wallet::detail;

void wallet_impl::scan_market_transaction(
        const market_transaction& mtrx,
        uint32_t block_num,
        const time_point_sec block_time
        )
{ try {
    auto okey_bid = _wallet_db.lookup_key( mtrx.bid_index.owner );
    if( okey_bid && okey_bid->has_private_key() )
    {
        const auto bid_account_key = _wallet_db.lookup_key( okey_bid->account_address );

        auto bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.bid_index.owner),
                                                                            mtrx.bid_index.order_price.base_asset_id ).get_address() );

        bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.bid_index.owner),
                                                                      mtrx.bid_index.order_price.quote_asset_id ).get_address() );

        /* Construct a unique record id */
        transaction_id_type record_id;
        {
            fc::ripemd160::encoder enc;
            fc::raw::pack( enc, block_num );
            fc::raw::pack( enc, mtrx.bid_type );
            fc::raw::pack( enc, mtrx.bid_index );
            fc::raw::pack( enc, mtrx.ask_type );
            fc::raw::pack( enc, mtrx.ask_index );
            record_id = enc.result();
        }

        // TODO: Don't blow away memo, etc.
        auto record = wallet_transaction_record();
        record.record_id = record_id;
        record.block_num = block_num;
        record.is_virtual = true;
        record.is_confirmed = true;
        record.is_market = true;
        record.created_time = block_time;
        record.received_time = block_time;

        if( mtrx.bid_type == bid_order )
        {
            {
                auto entry = ledger_entry();
                entry.from_account = okey_bid->public_key;
                //entry.to_account = "MARKET";
                entry.amount = mtrx.bid_paid;
                entry.memo = "pay bid @ " + _blockchain->to_pretty_price( mtrx.bid_index.order_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                entry.from_account = okey_bid->public_key;
                entry.to_account = bid_account_key->public_key;
                entry.amount = mtrx.bid_received;
                entry.memo = "bid proceeds @ " + _blockchain->to_pretty_price( mtrx.bid_index.order_price );
                record.ledger_entries.push_back( entry );
                self->wallet_claimed_transaction( entry );
            }
        }
        else /* if( mtrx.bid_type == short_order ) */
        {
            /* If not automatic market cancel */
            if( mtrx.ask_paid.amount != 0
                || mtrx.ask_received.amount != 0
                || mtrx.bid_received.asset_id != 0
                || mtrx.bid_paid.amount != 0 )
            {
                {
                    auto entry = ledger_entry();
                    entry.from_account = okey_bid->public_key;
                    entry.to_account = okey_bid->public_key;
                    if( mtrx.short_collateral.valid() )
                        entry.amount = *mtrx.short_collateral;
                    else
                        entry.amount = mtrx.bid_received;
                    entry.memo = "add collateral";
                    record.ledger_entries.push_back( entry );
                }
                {
                    auto entry = ledger_entry();
                    //entry.from_account = "MARKET";
                    entry.to_account =  okey_bid->public_key;
                    entry.amount = mtrx.ask_paid;
                    entry.memo = "add collateral";
                    record.ledger_entries.push_back( entry );
                }
                {
                    auto entry = ledger_entry();
                    entry.from_account = okey_bid->public_key;
                    //entry.to_account = "MARKET";
                    entry.amount = mtrx.bid_paid;
                    entry.memo = "short proceeds @ " + _blockchain->to_pretty_price( mtrx.bid_index.order_price );
                    record.ledger_entries.push_back( entry );
                }
            }
            else /* Automatic market cancel */
            {
                {
                    auto entry = ledger_entry();
                    entry.from_account = okey_bid->public_key;
                    entry.to_account = bid_account_key->public_key;
                    entry.amount = mtrx.bid_received;
                    entry.memo = "automatic market cancel";
                    record.ledger_entries.push_back( entry );
                }
            }
        }

        _wallet_db.store_transaction( record );
        _dirty_balances = true;
    }

    auto okey_ask = _wallet_db.lookup_key( mtrx.ask_index.owner );
    if( okey_ask && okey_ask->has_private_key() )
    {
        const auto ask_account_key = _wallet_db.lookup_key( okey_ask->account_address );

        auto bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.ask_index.owner),
                                                                            mtrx.ask_index.order_price.base_asset_id ).get_address() );

        bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.ask_index.owner),
                                                                      mtrx.ask_index.order_price.quote_asset_id ).get_address() );

        /* Construct a unique record id */
        transaction_id_type record_id;
        {
            fc::ripemd160::encoder enc;
            fc::raw::pack( enc, block_num );
            fc::raw::pack( enc, mtrx.ask_type );
            fc::raw::pack( enc, mtrx.ask_index );
            fc::raw::pack( enc, mtrx.bid_type );
            fc::raw::pack( enc, mtrx.bid_index );
            record_id = enc.result();
        }

        // TODO: Don't blow away memo, etc.
        auto record = wallet_transaction_record();
        record.record_id = record_id;
        record.block_num = block_num;
        record.is_virtual = true;
        record.is_confirmed = true;
        record.is_market = true;
        record.created_time = block_time;
        record.received_time = block_time;

        if( mtrx.ask_type == ask_order )
        {
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                //entry.to_account = "MARKET";
                entry.amount = mtrx.ask_paid;
                entry.memo = "fill ask @ " + _blockchain->to_pretty_price( mtrx.ask_index.order_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                entry.to_account = ask_account_key->public_key;
                entry.amount = mtrx.ask_received;
                entry.memo = "ask proceeds @ " + _blockchain->to_pretty_price( mtrx.ask_index.order_price );
                record.ledger_entries.push_back( entry );
                self->wallet_claimed_transaction( entry );
            }
        }
        else /* if( mtrx.ask_type == cover_order ) */
        {
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                //entry.to_account = "MARKET";
                entry.amount = mtrx.ask_paid;
                entry.memo = "sell collateral @ " + _blockchain->to_pretty_price( mtrx.ask_index.order_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                //entry.from_account = "MARKET";
                entry.to_account = okey_ask->public_key;
                entry.amount = mtrx.ask_received;
                entry.memo = "payoff debt @ " + _blockchain->to_pretty_price( mtrx.ask_index.order_price );
                record.ledger_entries.push_back( entry );
            }
            if( mtrx.returned_collateral.valid() )
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                entry.to_account = ask_account_key->public_key;
                entry.amount = *mtrx.returned_collateral;
                entry.memo = "cover proceeds";
                record.ledger_entries.push_back( entry );
                self->wallet_claimed_transaction( entry );
                record.fee = mtrx.base_fees;
            }
        }

        _wallet_db.store_transaction( record );
        _dirty_balances = true;
    }
} FC_CAPTURE_AND_RETHROW() }

// TODO: No longer needed with scan_genesis_experimental and get_account_balance_records
void wallet_impl::scan_balances()
{
    scan_balances_experimental();

   /* Delete ledger entries for any genesis balances before we can reconstruct them */
   const auto my_accounts = self->list_accounts();
   for( const auto& account : my_accounts )
   {
       const auto record_id = fc::ripemd160::hash( account.name );
       auto transaction_record = _wallet_db.lookup_transaction( record_id );
       if( transaction_record.valid() )
       {
           transaction_record->ledger_entries.clear();
           _wallet_db.store_transaction( *transaction_record );
       }
   }

   const auto timestamp = _blockchain->get_genesis_timestamp();
   const auto scan_balance = [&]( const balance_record& bal_rec )
   {
       if( !bal_rec.snapshot_info.valid() )
           return;

       if( bal_rec.condition.type != withdraw_signature_type )
           return;

       const auto owner = bal_rec.owner();
       if( !owner.valid() )
           return;

       const auto key_rec = _wallet_db.lookup_key( *owner );
       if( !key_rec.valid() || !key_rec->has_private_key() )
           return;

       const auto public_key = key_rec->public_key;
       auto record_id = fc::ripemd160::hash( self->get_key_label( public_key ) );
       auto transaction_record = _wallet_db.lookup_transaction( record_id );
       if( !transaction_record.valid() )
       {
           transaction_record = wallet_transaction_record();
           transaction_record->created_time = timestamp;
           transaction_record->received_time = timestamp;

           if( bal_rec.condition.type == withdraw_vesting_type )
               transaction_record->block_num = 933804;
       }

       auto entry = ledger_entry();
       entry.to_account = public_key;
       entry.amount = asset( bal_rec.snapshot_info->original_balance, bal_rec.condition.asset_id );
       entry.memo = "claim " + bal_rec.snapshot_info->original_address;

       transaction_record->record_id = record_id;
       transaction_record->is_virtual = true;
       transaction_record->is_confirmed = true;
       transaction_record->ledger_entries.push_back( entry );
       _wallet_db.store_transaction( *transaction_record );
   };
   _blockchain->scan_balances( scan_balance );
}

void wallet_impl::scan_block( uint32_t block_num )
{ try {
    const signed_block_header block_header = _blockchain->get_block_header( block_num );
    const vector<transaction_record> transaction_records = _blockchain->get_transactions_for_block( block_header.id() );
    for( const transaction_evaluation_state& eval_state : transaction_records )
    {
        try
        {
            scan_transaction( eval_state.trx, block_num, block_header.timestamp );
        }
        catch( ... )
        {
        }
    }

    const vector<market_transaction>& market_trxs = _blockchain->get_market_transactions( block_num );
    for( const market_transaction& market_trx : market_trxs )
    {
        try
        {
            scan_market_transaction( market_trx, block_num, block_header.timestamp );
        }
        catch( ... )
        {
        }
    }
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

wallet_transaction_record wallet_impl::scan_transaction(
        const signed_transaction& transaction,
        uint32_t block_num,
        const time_point_sec block_timestamp,
        bool overwrite_existing )
{ try {
    const transaction_id_type transaction_id = transaction.id();
    const transaction_id_type& record_id = transaction_id;
    auto transaction_record = _wallet_db.lookup_transaction( record_id );
    const auto already_exists = transaction_record.valid();
    if( !already_exists )
    {
        transaction_record = wallet_transaction_record();
        transaction_record->record_id = record_id;
        transaction_record->created_time = block_timestamp;
        transaction_record->received_time = block_timestamp;
        transaction_record->trx = transaction;
    }

    bool new_transaction = !transaction_record->is_confirmed;

    transaction_record->block_num = block_num;
    transaction_record->is_confirmed = true;

    if( already_exists ) /* Otherwise will get stored below if this is for me */
        _wallet_db.store_transaction( *transaction_record );

    auto store_record = false;

    /* Clear share amounts (but not asset ids) and we will reconstruct them below */
    for( auto& entry : transaction_record->ledger_entries )
    {
        if( entry.memo.find( "yield" ) == string::npos )
            entry.amount.amount = 0;
    }

    // Assume fees = withdrawals - deposits
    auto total_fee = asset( 0, 0 ); // Assume all fees paid in base asset

    public_key_type withdraw_pub_key;

    // Force scanning all withdrawals first because ledger reconstruction assumes such an ordering
    auto has_withdrawal = false;
    for( const auto& op : transaction.operations )
    {
        switch( operation_type_enum( op.type ) )
        {
            case withdraw_op_type:
                has_withdrawal |= scan_withdraw( op.as<withdraw_operation>(), *transaction_record, total_fee, withdraw_pub_key );
                break;
            case withdraw_pay_op_type:
                has_withdrawal |= scan_withdraw_pay( op.as<withdraw_pay_operation>(), *transaction_record, total_fee );
                break;
            case bid_op_type:
            {
                const auto bid_op = op.as<bid_operation>();
                if( bid_op.amount < 0 )
                    has_withdrawal |= scan_bid( bid_op, *transaction_record, total_fee );
                break;
            }
            case ask_op_type:
            {
                const auto ask_op = op.as<ask_operation>();
                if( ask_op.amount < 0 )
                    has_withdrawal |= scan_ask( ask_op, *transaction_record, total_fee );
                break;
            }
            case short_op_v2_type:
            {
                const auto short_op = op.as<short_operation>();
                if( short_op.amount < 0 )
                    has_withdrawal |= scan_short( short_op, *transaction_record, total_fee );
                break;
            }
            case short_op_type:
            {
                const auto short_op = op.as<short_operation_v1>();
                if( short_op.amount < 0 )
                    has_withdrawal |= scan_short_v1( short_op, *transaction_record, total_fee );
                break;
            }
            default:
                break;
        }
    }
    store_record |= has_withdrawal;


    // Force scanning all deposits next because ledger reconstruction assumes such an ordering
    auto has_deposit = false;
    bool is_deposit = false;
    for( const auto& op : transaction.operations )
    {
        switch( operation_type_enum( op.type ) )
        {
            case deposit_op_type:
            {
                is_deposit = scan_deposit( op.as<deposit_operation>(), *transaction_record, total_fee );
                has_deposit |= is_deposit;
                break;
            }
            case bid_op_type:
            {
                const auto bid_op = op.as<bid_operation>();
                if( bid_op.amount >= 0 )
                    has_deposit |= scan_bid( bid_op, *transaction_record, total_fee );
                break;
            }
            case ask_op_type:
            {
                const auto ask_op = op.as<ask_operation>();
                if( ask_op.amount >= 0 )
                    has_deposit |= scan_ask( ask_op, *transaction_record, total_fee );
                break;
            }
            case short_op_v2_type:
            {
                const auto short_op = op.as<short_operation>();
                if( short_op.amount >= 0 )
                    has_deposit |= scan_short( short_op, *transaction_record, total_fee );
                break;
            }
            case short_op_type:
            {
                const auto short_op = op.as<short_operation_v1>();
                if( short_op.amount >= 0 )
                    has_deposit |= scan_short_v1( short_op, *transaction_record, total_fee );
                break;
            }
            case burn_op_type:
            {
                store_record |= scan_burn( op.as<burn_operation>(), *transaction_record, total_fee );
                break;
            }
            default:
                break;
        }
    }
    store_record |= has_deposit;

    if( new_transaction && is_deposit && transaction_record && transaction_record->ledger_entries.size() )
        self->wallet_claimed_transaction( transaction_record->ledger_entries.back() );

    /* Reconstruct fee */
    if( has_withdrawal && !has_deposit )
    {
        for( auto& entry : transaction_record->ledger_entries )
        {
            if( entry.amount.asset_id == total_fee.asset_id )
                entry.amount -= total_fee;
        }

    }
    transaction_record->fee = total_fee;

    /* When the only withdrawal for asset 0 is the fee (bids) */
    if( transaction_record->ledger_entries.size() > 1 )
    {
        const auto entries = transaction_record->ledger_entries;
        transaction_record->ledger_entries.clear();
        for( const auto& entry : entries )
        {
            if( entry.amount != transaction_record->fee )
                transaction_record->ledger_entries.push_back( entry );
        }

    }

    for( const auto& op : transaction.operations )
    {
        switch( operation_type_enum( op.type ) )
        {
            case register_account_op_type:
                store_record |= scan_register_account( op.as<register_account_operation>(), *transaction_record );
                break;
            case update_account_op_type:
                store_record |= scan_update_account( op.as<update_account_operation>(), *transaction_record );
                break;
            case create_asset_op_type:
                store_record |= scan_create_asset( op.as<create_asset_operation>(), *transaction_record );
                break;
            case issue_asset_op_type:
                store_record |= scan_issue_asset( op.as<issue_asset_operation>(), *transaction_record );
                break;
            case update_feed_op_type:
                store_record |= scan_update_feed( op.as<update_feed_operation>(), *transaction_record );
                break;
            default:
                break;
        }
    }

    if( has_withdrawal )
    {
       auto blockchain_trx_state = _blockchain->get_transaction( transaction_id );
       if( blockchain_trx_state.valid() )
       {
          if( !transaction_record->ledger_entries.empty() )
          {
              /* Remove all yield entries and re-add them */
              while( !transaction_record->ledger_entries.empty()
                     && transaction_record->ledger_entries.back().memo.find( "yield" ) == 0 )
              {
                  transaction_record->ledger_entries.pop_back();
              }

              for( const auto& yield_item : blockchain_trx_state->yield_claimed )
              {
                 auto entry = ledger_entry();
                 entry.amount = asset( yield_item.second, yield_item.first );
                 entry.to_account = withdraw_pub_key;
                 entry.from_account = withdraw_pub_key;
                 entry.memo = "yield";
                 transaction_record->ledger_entries.push_back( entry );
                 self->wallet_claimed_transaction( transaction_record->ledger_entries.back() );
              }

              if( !blockchain_trx_state->yield_claimed.empty() )
                 _wallet_db.store_transaction( *transaction_record );
          }

          if( blockchain_trx_state->fees_paid.count( transaction_record->fee.asset_id ) > 0 )
              transaction_record->fee.amount = blockchain_trx_state->fees_paid.at( transaction_record->fee.asset_id );
       }
    }

    /* Only overwrite existing record if you did not create it or overwriting was explicitly specified */
    if( store_record && ( !already_exists || overwrite_existing ) )
        _wallet_db.store_transaction( *transaction_record );

    _dirty_balances |= store_record;

    return *transaction_record;
} FC_CAPTURE_AND_RETHROW() }

// TODO: Refactor scan_withdraw{_pay}; almost exactly the same
bool wallet_impl::scan_withdraw( const withdraw_operation& op,
                                 wallet_transaction_record& trx_rec, asset& total_fee,
                                 public_key_type& withdraw_pub_key )
{ try {
   const auto bal_rec = _blockchain->get_balance_record( op.balance_id );
   FC_ASSERT( bal_rec.valid() );
   const auto amount = asset( op.amount, bal_rec->condition.asset_id );

   if( amount.asset_id == total_fee.asset_id )
      total_fee += amount;

   const auto owner = bal_rec->owner();
   if( !owner.valid() )
       return false;

   const auto key_rec =_wallet_db.lookup_key( *owner );
   if( key_rec.valid() && key_rec->has_private_key() ) /* If we own this balance */
   {
       auto new_entry = true;
       for( auto& entry : trx_rec.ledger_entries )
       {
           if( !entry.from_account.valid() ) continue;
           const auto a1 = self->get_account_for_address( *entry.from_account );
           if( !a1.valid() ) continue;
           const auto a2 = self->get_account_for_address( key_rec->account_address );
           if( !a2.valid() ) continue;
           if( a1->name != a2->name ) continue;

           // TODO: We should probably really have a map of asset ids to amounts per ledger entry
           if( entry.amount.asset_id == amount.asset_id )
           {
               entry.amount += amount;
               new_entry = false;
               break;
           }
           else if( entry.amount.amount == 0 )
           {
               if( amount != trx_rec.fee )
                   entry.amount = amount;
               new_entry = false;
               break;
           }
       }
       if( new_entry )
       {
           auto entry = ledger_entry();
           entry.from_account = key_rec->public_key;
           entry.amount = amount;
           trx_rec.ledger_entries.push_back( entry );
       }
       withdraw_pub_key = key_rec->public_key;

       return true;
   }
   return false;
} FC_CAPTURE_AND_RETHROW() }

// TODO: Refactor scan_withdraw{_pay}; almost exactly the same
bool wallet_impl::scan_withdraw_pay( const withdraw_pay_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
   const auto amount = asset( op.amount ); // Always base asset

   if( amount.asset_id == total_fee.asset_id )
       total_fee += amount;

   const auto account_rec = _blockchain->get_account_record( op.account_id );
   FC_ASSERT( account_rec.valid() );
   const auto key_rec =_wallet_db.lookup_key( account_rec->owner_key );
   if( key_rec.valid() && key_rec->has_private_key() ) /* If we own this account */
   {
       auto new_entry = true;
       for( auto& entry : trx_rec.ledger_entries )
       {
            if( !entry.from_account.valid() ) continue;
            const auto a1 = self->get_account_for_address( *entry.from_account );
            if( !a1.valid() ) continue;
            const auto a2 = self->get_account_for_address( key_rec->account_address );
            if( !a2.valid() ) continue;
            if( a1->name != a2->name ) continue;

            // TODO: We should probably really have a map of asset ids to amounts per ledger entry
            if( entry.amount.asset_id == amount.asset_id )
            {
                entry.amount += amount;
                if( entry.memo.empty() ) entry.memo = "withdraw pay";
                new_entry = false;
                break;
            }
            else if( entry.amount.amount == 0 )
            {
                entry.amount = amount;
                if( entry.memo.empty() ) entry.memo = "withdraw pay";
                new_entry = false;
                break;
            }
       }
       if( new_entry )
       {
           auto entry = ledger_entry();
           entry.from_account = key_rec->public_key;
           entry.amount = amount;
           entry.memo = "withdraw pay";
           trx_rec.ledger_entries.push_back( entry );
       }

       return true;
   }
   return false;
} FC_CAPTURE_AND_RETHROW() }

bool wallet_impl::scan_register_account( const register_account_operation& op, wallet_transaction_record& trx_rec )
{
    auto opt_key_rec = _wallet_db.lookup_key( op.owner_key );

    if( !opt_key_rec.valid() || !opt_key_rec->has_private_key() )
       return false;

    auto opt_account = _wallet_db.lookup_account( address( op.owner_key ) );
    if( !opt_account.valid() )
    {
       wlog( "We have the key but no account for registration operation" );
       return false;
    }

    wlog( "we detected an account register operation for ${name}", ("name",op.name) );
    auto account_name_rec = _blockchain->get_account_record( op.name );
    FC_ASSERT( account_name_rec.valid() );

    for( auto& entry : trx_rec.ledger_entries )
    {
        if( !entry.to_account.valid() )
        {
            entry.to_account = op.owner_key;
            entry.amount = asset( 0 ); // Assume scan_withdraw came first
            entry.memo = "register " + account_name_rec->name; // Can't tell if initially registered as a delegate
            break;
        }
        else if( entry.to_account == op.owner_key )
        {
            entry.amount = asset( 0 ); // Assume scan_withdraw came first
            break;
        }
    }

    _dirty_accounts = true;
    return true;
}

bool wallet_impl::scan_update_account( const update_account_operation& op, wallet_transaction_record& trx_rec )
{ try {
    auto oaccount =  _blockchain->get_account_record( op.account_id );
    FC_ASSERT( oaccount.valid() );
    auto opt_key_rec = _wallet_db.lookup_key( oaccount->owner_key );
    if( !opt_key_rec.valid() )
       return false;

    auto opt_account = _wallet_db.lookup_account( address( oaccount->owner_key ) );
    if( !opt_account.valid() )
    {
       wlog( "We have the key but no account for update operation" );
       return false;
    }
    wlog( "we detected an account update operation for ${name}", ("name",oaccount->name) );
    auto account_name_rec = _blockchain->get_account_record( oaccount->name );
    FC_ASSERT( account_name_rec.valid() );

    for( auto& entry : trx_rec.ledger_entries )
    {
        if( !entry.to_account.valid() )
        {
            entry.to_account = oaccount->owner_key;
            entry.amount = asset( 0 ); // Assume scan_withdraw came first
            entry.memo = "update " + oaccount->name;
            break;
        }
        else if( entry.to_account == oaccount->owner_key )
        {
            entry.amount = asset( 0 ); // Assume scan_withdraw came first
            break;
        }
    }

    _dirty_accounts = true;
    return true;
} FC_CAPTURE_AND_RETHROW() }

bool wallet_impl::scan_create_asset( const create_asset_operation& op, wallet_transaction_record& trx_rec )
{
   if( op.issuer_account_id != asset_record::market_issuer_id )
   {
      auto oissuer = _blockchain->get_account_record( op.issuer_account_id );
      FC_ASSERT( oissuer.valid() );
      auto opt_key_rec = _wallet_db.lookup_key( oissuer->owner_key );
      if( opt_key_rec.valid() && opt_key_rec->has_private_key() )
      {
         for( auto& entry : trx_rec.ledger_entries )
         {
             if( !entry.to_account.valid() )
             {
                 entry.to_account = oissuer->owner_key;
                 entry.amount = asset( 0 ); // Assume scan_withdraw came first
                 entry.memo = "create " + op.symbol + " (" + op.name + ")";
                 return true;
             }
             else if( entry.to_account == oissuer->owner_key )
             {
                 entry.amount = asset( 0 ); // Assume scan_withdraw came first
                 return true;
             }
         }
      }
   }
   return false;
}

bool wallet_impl::scan_issue_asset( const issue_asset_operation& op, wallet_transaction_record& trx_rec )
{
   for( auto& entry : trx_rec.ledger_entries )
   {
       if( entry.from_account.valid() )
       {
           const auto opt_key_rec = _wallet_db.lookup_key( *entry.from_account );
           if( opt_key_rec.valid() && opt_key_rec->has_private_key() )
           {
               entry.amount = op.amount;
               entry.memo = "issue " + _blockchain->to_pretty_asset( op.amount );
               return true;
           }
       }
   }
   return false;
}

bool wallet_impl::scan_update_feed( const update_feed_operation& op, wallet_transaction_record& trx_rec )
{
   for( auto& entry : trx_rec.ledger_entries )
   {
      if( entry.from_account.valid() )
      {
         const auto opt_key_rec = _wallet_db.lookup_key( *entry.from_account );
         if( opt_key_rec.valid() && opt_key_rec->has_private_key() )
         {
            entry.memo = "update feeds for " + self->get_key_label(*entry.from_account);
            entry.to_account = entry.from_account;
            return true;
         }
      }
   }
   return false;
}

// TODO: Refactor scan_{bid|ask|short}; exactly the same
bool wallet_impl::scan_bid( const bid_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    const auto amount = op.get_amount();
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    auto okey_rec = _wallet_db.lookup_key( op.bid_index.owner );
    if( okey_rec.valid() && okey_rec->has_private_key() )
    {
       /* Restore key label */
       const market_order order( bid_order, op.bid_index, op.amount );
       okey_rec->memo = order.get_small_id();
       _wallet_db.store_key( *okey_rec );

       for( auto& entry : trx_rec.ledger_entries )
       {
           if( amount.amount >= 0 )
           {
               if( !entry.to_account.valid() )
               {
                   entry.to_account = okey_rec->public_key;
                   entry.amount = amount;
                   //entry.memo =
                   break;
               }
               else if( *entry.to_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   break;
               }
           }
           else /* Cancel order */
           {
               if( !entry.from_account.valid() )
               {
                   entry.from_account = okey_rec->public_key;
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
               else if( *entry.from_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
           }
       }

       return true;
    }
    return false;
} FC_CAPTURE_AND_RETHROW( (op) ) }

// TODO: Refactor scan_{bid|ask|short}; exactly the same
bool wallet_impl::scan_ask( const ask_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    const auto amount = op.get_amount();
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    auto okey_rec = _wallet_db.lookup_key( op.ask_index.owner );
    if( okey_rec.valid() && okey_rec->has_private_key() )
    {
       /* Restore key label */
       const market_order order( ask_order, op.ask_index, op.amount );
       okey_rec->memo = order.get_small_id();
       _wallet_db.store_key( *okey_rec );

       for( auto& entry : trx_rec.ledger_entries )
       {
           if( amount.amount >= 0 )
           {
               if( !entry.to_account.valid() )
               {
                   entry.to_account = okey_rec->public_key;
                   entry.amount = amount;
                   //entry.memo =
                   break;
               }
               else if( *entry.to_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   break;
               }
           }
           else /* Cancel order */
           {
               if( !entry.from_account.valid() )
               {
                   entry.from_account = okey_rec->public_key;
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
               else if( *entry.from_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
           }
       }

       return true;
    }
    return false;
} FC_CAPTURE_AND_RETHROW( (op) ) }

// TODO: Refactor scan_{bid|ask|short}; exactly the same
bool wallet_impl::scan_short( const short_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    const auto amount = op.get_amount();
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    auto okey_rec = _wallet_db.lookup_key( op.short_index.owner );
    if( okey_rec.valid() && okey_rec->has_private_key() )
    {
       /* Restore key label */
       const market_order order( short_order, op.short_index, op.amount );
       okey_rec->memo = order.get_small_id();
       _wallet_db.store_key( *okey_rec );

       for( auto& entry : trx_rec.ledger_entries )
       {
           if( amount.amount >= 0 )
           {
               if( !entry.to_account.valid() )
               {
                   entry.to_account = okey_rec->public_key;
                   entry.amount = amount;
                   //entry.memo =
                   break;
               }
               else if( *entry.to_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   break;
               }
           }
           else /* Cancel order */
           {
               if( !entry.from_account.valid() )
               {
                   entry.from_account = okey_rec->public_key;
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
               else if( *entry.from_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
           }
       }

       return true;
    }
    return false;
} FC_CAPTURE_AND_RETHROW( (op) ) }

bool wallet_impl::scan_short_v1( const short_operation_v1& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    const auto amount = op.get_amount();
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    auto okey_rec = _wallet_db.lookup_key( op.short_index.owner );
    if( okey_rec.valid() && okey_rec->has_private_key() )
    {
       /* Restore key label */
       const market_order order( short_order, op.short_index, op.amount );
       okey_rec->memo = order.get_small_id();
       _wallet_db.store_key( *okey_rec );

       for( auto& entry : trx_rec.ledger_entries )
       {
           if( amount.amount >= 0 )
           {
               if( !entry.to_account.valid() )
               {
                   entry.to_account = okey_rec->public_key;
                   entry.amount = amount;
                   //entry.memo =
                   break;
               }
               else if( *entry.to_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   break;
               }
           }
           else /* Cancel order */
           {
               if( !entry.from_account.valid() )
               {
                   entry.from_account = okey_rec->public_key;
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
               else if( *entry.from_account == okey_rec->public_key )
               {
                   entry.amount = amount;
                   entry.memo = "cancel " + *okey_rec->memo;
                   break;
               }
           }
       }

       return true;
    }
    return false;
} FC_CAPTURE_AND_RETHROW( (op) ) }

bool wallet_impl::scan_burn( const burn_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{
    if( op.amount.asset_id == total_fee.asset_id )
        total_fee -= op.amount;

    if( trx_rec.ledger_entries.size() == 1 )
    {
        //trx_rec.ledger_entries.front().amount = op.amount;
        trx_rec.ledger_entries.front().memo = "burn";
        if( !op.message.empty() )
            trx_rec.ledger_entries.front().memo += ": " + op.message;
    }

    return false;
}

bool wallet_impl::scan_deposit( const deposit_operation& op, wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    auto amount = asset( op.amount, op.condition.asset_id );
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    bool cache_deposit = false;
    switch( (withdraw_condition_types) op.condition.type )
    {
       case withdraw_signature_type:
       {
          const auto deposit = op.condition.as<withdraw_with_signature>();
          if( deposit.memo )
          {
              omemo_status status;
              optional<private_key_type> recipient_key;
              try
              {
                  const private_key_type private_key = self->get_private_key( deposit.owner );
                  status = deposit.decrypt_memo_data( private_key, true );
                  if( status.valid() ) recipient_key = private_key;
              }
              catch( const fc::exception& )
              {
              }

              if( !status.valid() )
              {
                  vector<fc::future<void>> scan_key_progress;
                  scan_key_progress.resize( _stealth_private_keys.size() );
                  for( uint32_t i = 0; i < _stealth_private_keys.size(); ++i )
                  {
                     scan_key_progress[ i ] = fc::async( [ &, i ]()
                     {
                         _scanner_threads[ i % _num_scanner_threads ]->async( [ & ]()
                         {
                             if( !status.valid() )
                             {
                                 const omemo_status inner_status = deposit.decrypt_memo_data( _stealth_private_keys.at( i ) );
                                 if( inner_status.valid() )
                                 {
                                     status = inner_status;
                                     recipient_key = _stealth_private_keys.at( i );
                                 }
                             }
                         }, "decrypt memo" ).wait();
                     } );
                  } // for each key

                  for( auto& fut : scan_key_progress )
                  {
                     try
                     {
                        fut.wait();
                     }
                     catch( const fc::exception& e )
                     {
                     }
                  }
              }

              /* If I've successfully decrypted then it's for me */
              if( status.valid() && recipient_key.valid() )
              {
                 cache_deposit = true;
                 _wallet_db.cache_memo( *status, *recipient_key, _wallet_password );

                 auto new_entry = true;
                 if( status->memo_flags == from_memo )
                 {
                    for( auto& entry : trx_rec.ledger_entries )
                    {
                        if( !entry.from_account.valid() ) continue;
                        if( !entry.memo_from_account.valid() )
                        {
                            const auto a1 = self->get_key_label( *entry.from_account );
                            const auto a2 = self->get_key_label( status->from );
                            if( a1 != a2 ) continue;
                        }

                        new_entry = false;
                        if( !entry.memo_from_account.valid() )
                            entry.from_account = status->from;
                        entry.to_account = recipient_key->get_public_key();
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        break;
                    }
                    if( new_entry )
                    {
                        auto entry = ledger_entry();
                        entry.from_account = status->from;
                        entry.to_account = recipient_key->get_public_key();
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        trx_rec.ledger_entries.push_back( entry );
                    }
                 }
                 else // to_memo
                 {
                    for( auto& entry : trx_rec.ledger_entries )
                    {
                        if( !entry.from_account.valid() ) continue;
                        const auto a1 = self->get_key_label( *entry.from_account );
                        const auto a2 = self->get_key_label( recipient_key->get_public_key() );
                        if( a1 != a2 ) continue;

                        new_entry = false;
                        entry.from_account = recipient_key->get_public_key();
                        entry.to_account = status->from;
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        break;
                    }
                    if( new_entry )
                    {
                        auto entry = ledger_entry();
                        entry.from_account = recipient_key->get_public_key();
                        entry.to_account = status->from;
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        trx_rec.ledger_entries.push_back( entry );
                    }
                 }
              }
          }
          else /* non-TITAN with no memo, market cancel, or cover proceeds */
          {
              const auto okey_rec = _wallet_db.lookup_key( deposit.owner );
              if( okey_rec && okey_rec->has_private_key() )
              {
                  bool new_entry = true;
                  cache_deposit = true;
                  for( auto& entry : trx_rec.ledger_entries )
                  {
                      if( !entry.from_account.valid() ) continue;
                      const auto account_rec = self->get_account_for_address( okey_rec->public_key );
                      if( !account_rec.valid() ) continue;
                      const auto account_key_rec = _wallet_db.lookup_key( account_rec->owner_address() );
                      if( !account_key_rec.valid() ) continue;
                      if( !trx_rec.trx.is_cancel() ) /* cover proceeds */
                      {
                          if( entry.amount.asset_id != amount.asset_id ) continue;
                      }
                      entry.to_account = account_key_rec->public_key;
                      entry.amount = amount;
                      //entry.memo =
                      if( !trx_rec.trx.is_cancel() ) /* cover proceeds */
                      {
                          if( amount.asset_id == total_fee.asset_id )
                              total_fee += amount;
                      }
                      new_entry = false;
                      break;
                  }
                  if( new_entry )
                  {
                      auto entry = ledger_entry();
                      //entry.from_account = okey_rec->public_key;
                      entry.to_account = okey_rec->public_key;
                      entry.amount = amount;
                      trx_rec.ledger_entries.push_back( entry );
                  }
              }
          }
          break;
       }

       case withdraw_escrow_type:
       {
          const auto deposit = op.condition.as<withdraw_with_escrow>();
          if( deposit.memo )
          {
              omemo_status status;
              optional<private_key_type> recipient_key;
              for( const address& owner : op.condition.owners() )
              {
                  try
                  {
                      const private_key_type private_key = self->get_private_key( owner );
                      status = deposit.decrypt_memo_data( private_key, true );
                      if( status.valid() )
                      {
                          recipient_key = private_key;
                          break;
                      }
                  }
                  catch( const fc::exception& )
                  {
                  }
              }

              if( !status.valid() )
              {
                  vector<fc::future<void>> scan_key_progress;
                  scan_key_progress.resize( _stealth_private_keys.size() );
                  for( uint32_t i = 0; i < _stealth_private_keys.size(); ++i )
                  {
                     scan_key_progress[ i ] = fc::async( [ &, i ]()
                     {
                         _scanner_threads[ i % _num_scanner_threads ]->async( [ & ]()
                         {
                             if( !status.valid() )
                             {
                                 const omemo_status inner_status = deposit.decrypt_memo_data( _stealth_private_keys.at( i ) );
                                 if( inner_status.valid() )
                                 {
                                     status = inner_status;
                                     recipient_key = _stealth_private_keys.at( i );
                                 }
                             }
                         }, "decrypt memo" ).wait();
                     } );
                  } // for each key

                  for( auto& fut : scan_key_progress )
                  {
                     try
                     {
                        fut.wait();
                     }
                     catch( const fc::exception& e )
                     {
                     }
                  }
              }

              /* If I've successfully decrypted then it's for me */
              if( status.valid() && recipient_key.valid() )
              {
                 cache_deposit = true;
                 _wallet_db.cache_memo( *status, *recipient_key, _wallet_password );

                 auto new_entry = true;
                 if( status->memo_flags == from_memo )
                 {
                    for( auto& entry : trx_rec.ledger_entries )
                    {
                        if( !entry.from_account.valid() ) continue;
                        if( !entry.memo_from_account.valid() )
                        {
                            const auto a1 = self->get_key_label( *entry.from_account );
                            const auto a2 = self->get_key_label( status->from );
                            if( a1 != a2 ) continue;
                        }

                        new_entry = false;
                        if( !entry.memo_from_account.valid() )
                            entry.from_account = status->from;
                        entry.to_account = recipient_key->get_public_key();
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        break;
                    }
                    if( new_entry )
                    {
                        auto entry = ledger_entry();
                        entry.from_account = status->from;
                        entry.to_account = recipient_key->get_public_key();
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        trx_rec.ledger_entries.push_back( entry );
                    }
                 }
                 else // to_memo
                 {
                    for( auto& entry : trx_rec.ledger_entries )
                    {
                        if( !entry.from_account.valid() ) continue;
                        const auto a1 = self->get_key_label( *entry.from_account );
                        const auto a2 = self->get_key_label( recipient_key->get_public_key() );
                        if( a1 != a2 ) continue;

                        new_entry = false;
                        entry.from_account = recipient_key->get_public_key();
                        entry.to_account = status->from;
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        break;
                    }
                    if( new_entry )
                    {
                        auto entry = ledger_entry();
                        entry.from_account = recipient_key->get_public_key();
                        entry.to_account = status->from;
                        entry.amount = amount;
                        entry.memo = status->get_message();
                        trx_rec.ledger_entries.push_back( entry );
                    }
                 }
              }
          }
          else /* non-TITAN with no memo, market cancel, or cover proceeds */
          {
              for( const address& owner : op.condition.owners() )
              {
                  const auto okey_rec = _wallet_db.lookup_key( owner );
                  if( okey_rec && okey_rec->has_private_key() )
                  {
                      bool new_entry = true;
                      cache_deposit = true;
                      for( auto& entry : trx_rec.ledger_entries )
                      {
                          if( !entry.from_account.valid() ) continue;
                          const auto account_rec = self->get_account_for_address( okey_rec->public_key );
                          if( !account_rec.valid() ) continue;
                          const auto account_key_rec = _wallet_db.lookup_key( account_rec->owner_address() );
                          if( !account_key_rec.valid() ) continue;
                          if( !trx_rec.trx.is_cancel() ) /* cover proceeds */
                          {
                              if( entry.amount.asset_id != amount.asset_id ) continue;
                          }
                          entry.to_account = account_key_rec->public_key;
                          entry.amount = amount;
                          //entry.memo =
                          if( !trx_rec.trx.is_cancel() ) /* cover proceeds */
                          {
                              if( amount.asset_id == total_fee.asset_id )
                                  total_fee += amount;
                          }
                          new_entry = false;
                          break;
                      }
                      if( new_entry )
                      {
                          auto entry = ledger_entry();
                          //entry.from_account = okey_rec->public_key;
                          entry.to_account = okey_rec->public_key;
                          entry.amount = amount;
                          trx_rec.ledger_entries.push_back( entry );
                      }
                  }
              }
          }
          break;
       }

       default:
       {
          break;
       }
  }

  return cache_deposit;
} FC_CAPTURE_AND_RETHROW() }

wallet_transaction_record wallet::scan_transaction( const string& transaction_id_prefix, bool overwrite_existing )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   if( transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string( transaction_id_type() ).size() )
       FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix",transaction_id_prefix) );

   const auto transaction_id = variant( transaction_id_prefix ).as<transaction_id_type>();
   const auto transaction_record = my->_blockchain->get_transaction( transaction_id, false );
   if( !transaction_record.valid() )
       FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );

   my->scan_accounts();

   const auto block_num = transaction_record->chain_location.block_num;
   const auto block = my->_blockchain->get_block_header( block_num );
   const auto record = my->scan_transaction( transaction_record->trx, block_num, block.timestamp, overwrite_existing );

   if( my->_dirty_balances ) my->scan_balances_experimental();
   if( my->_dirty_accounts ) my->scan_accounts();

   return record;
} FC_CAPTURE_AND_RETHROW() }

vector<wallet_transaction_record> wallet::get_transactions( const string& transaction_id_prefix )
{ try {
   FC_ASSERT( is_open() );

   if( transaction_id_prefix.size() > string( transaction_id_type() ).size() )
       FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix",transaction_id_prefix) );

   auto transactions = vector<wallet_transaction_record>();
   const auto records = my->_wallet_db.get_transactions();
   for( const auto& record : records )
   {
       const auto transaction_id = string( record.first );
       if( string( transaction_id ).find( transaction_id_prefix ) != 0 ) continue;
       transactions.push_back( record.second );
   }
   return transactions;
} FC_CAPTURE_AND_RETHROW() }

void wallet_impl::sign_transaction( signed_transaction& transaction, const unordered_set<address>& required_signatures )const
{ try {
    const auto chain_id = _blockchain->get_chain_id();
    for( const auto& addr : required_signatures )
        transaction.sign( self->get_private_key( addr ), chain_id );
} FC_CAPTURE_AND_RETHROW( (transaction)(required_signatures) ) }

void wallet::cache_transaction( wallet_transaction_record& transaction_record )
{ try {
   my->_blockchain->store_pending_transaction( transaction_record.trx, true );
   my->scan_balances_experimental();

   transaction_record.record_id = transaction_record.trx.id();
   transaction_record.created_time = blockchain::now();
   transaction_record.received_time = transaction_record.created_time;
   my->_wallet_db.store_transaction( transaction_record );
} FC_CAPTURE_AND_RETHROW( (transaction_record) ) }

/**
 * @return the list of all transactions related to this wallet
 */
vector<wallet_transaction_record> wallet::get_transaction_history( const string& account_name,
                                                                   uint32_t start_block_num,
                                                                   uint32_t end_block_num,
                                                                   const string& asset_symbol )const
{ try {
   FC_ASSERT( is_open() );
   if( end_block_num != -1 ) FC_ASSERT( start_block_num <= end_block_num );

   vector<wallet_transaction_record> history_records;
   const auto& transactions = my->_wallet_db.get_transactions();

   asset_id_type asset_id = 0;
   if( !asset_symbol.empty() && asset_symbol != BTS_BLOCKCHAIN_SYMBOL )
   {
       try
       {
           asset_id = my->_blockchain->get_asset_id( asset_symbol );
       }
       catch( const fc::exception& )
       {
           FC_THROW_EXCEPTION( invalid_asset_symbol, "Invalid asset symbol!", ("asset_symbol",asset_symbol) );
       }
   }

   for( const auto& item : transactions )
   {
       const auto& tx_record = item.second;

       if( tx_record.block_num < start_block_num ) continue;
       if( end_block_num != -1 && tx_record.block_num > end_block_num ) continue;
       if( tx_record.ledger_entries.empty() ) continue; /* TODO: Temporary */

       if( !account_name.empty() )
       {
           bool match = false;
           for( const auto& entry : tx_record.ledger_entries )
           {
               if( entry.from_account.valid() )
               {
                   const auto account_record = get_account_for_address( *entry.from_account );
                   if( account_record.valid() ) match |= account_record->name == account_name;
                   if( match ) break;
               }
               if( entry.to_account.valid() )
               {
                   const auto account_record = get_account_for_address( *entry.to_account );
                   if( account_record.valid() ) match |= account_record->name == account_name;
                   if( match ) break;
               }
           }
           if( !match ) continue;
       }

       if( asset_id != 0 )
       {
           bool match = false;
           for( const auto& entry : tx_record.ledger_entries )
               match |= entry.amount.amount > 0 && entry.amount.asset_id == asset_id;
           match |= tx_record.fee.amount > 0 && tx_record.fee.asset_id == asset_id;
           if( !match ) continue;
       }

       history_records.push_back( tx_record );
   }

   return history_records;
} FC_CAPTURE_AND_RETHROW() }

vector<pretty_transaction> wallet::get_pretty_transaction_history( const string& account_name,
                                                                   uint32_t start_block_num,
                                                                   uint32_t end_block_num,
                                                                   const string& asset_symbol )const
{ try {

    // TODO: Validate all input

    const auto& history = get_transaction_history( account_name, start_block_num, end_block_num, asset_symbol );
    vector<pretty_transaction> pretties;
    pretties.reserve( history.size() );
    for( const auto& item : history ) pretties.push_back( to_pretty_trx( item ) );

    const auto sorter = []( const pretty_transaction& a, const pretty_transaction& b ) -> bool
    {
        if( a.is_confirmed == b.is_confirmed && a.block_num != b.block_num )
            return a.block_num < b.block_num;

        if( a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp;

        return string( a.trx_id ).compare( string( b.trx_id ) ) < 0;
    };
    std::sort( pretties.begin(), pretties.end(), sorter );

    // TODO: Handle pagination

    const auto errors = get_pending_transaction_errors();
    for( auto& trx : pretties )
    {
        if( trx.is_virtual || trx.is_confirmed ) continue;
        if( errors.count( trx.trx_id ) <= 0 ) continue;
        const auto trx_rec = my->_blockchain->get_transaction( trx.trx_id );
        if( trx_rec.valid() )
        {
            trx.block_num = trx_rec->chain_location.block_num;
            trx.is_confirmed = true;
            continue;
        }
        trx.error = errors.at( trx.trx_id );
    }

    vector<string> account_names;
    bool account_specified = !account_name.empty();
    if( !account_specified )
    {
        const auto accounts = list_accounts();
        for( const auto& account : accounts )
            account_names.push_back( account.name );
    }
    else
    {
        account_names.push_back( account_name );
    }

    /* Tally up running balances */
    const bool end_before_head = end_block_num != -1
                                 && end_block_num <= my->_blockchain->get_head_block_num();
    const fc::time_point_sec now( my->_blockchain->now() );
    for( const auto& name : account_names )
    {
        map<asset_id_type, asset> running_balances;
        for( auto& trx : pretties )
        {
            if( !trx.is_virtual && !trx.is_confirmed
                    && ( end_before_head || trx.expiration_timestamp < now ) )
            {
                continue;
            }
            const auto fee_asset_id = trx.fee.asset_id;
            if( running_balances.count( fee_asset_id ) <= 0 )
                running_balances[ fee_asset_id ] = asset( 0, fee_asset_id );

            auto any_from_me = false;
            for( auto& entry : trx.ledger_entries )
            {
                const auto amount_asset_id = entry.amount.asset_id;
                if( running_balances.count( amount_asset_id ) <= 0 )
                    running_balances[ amount_asset_id ] = asset( 0, amount_asset_id );

                auto from_me = false;
                from_me |= name == entry.from_account;
                from_me |= ( entry.from_account.find( name + " " ) == 0 ); /* If payer != sender */
                if( from_me )
                {
                    /* Special check to ignore asset issuing */
                    if( ( running_balances[ amount_asset_id ] - entry.amount ) >= asset( 0, amount_asset_id ) )
                        running_balances[ amount_asset_id ] -= entry.amount;

                    /* Subtract fee once on the first entry */
                    if( !trx.is_virtual && !any_from_me )
                        running_balances[ fee_asset_id ] -= trx.fee;
                }
                any_from_me |= from_me;

                /* Special case to subtract fee if we canceled a bid */
                if( !trx.is_virtual && trx.is_market_cancel && amount_asset_id != fee_asset_id )
                    running_balances[ fee_asset_id ] -= trx.fee;

                auto to_me = false;
                to_me |= name == entry.to_account;
                to_me |= ( entry.to_account.find( name + " " ) == 0 ); /* If payer != sender */
                if( to_me ) running_balances[ amount_asset_id ] += entry.amount;

                entry.running_balances[ name ][ amount_asset_id ] = running_balances[ amount_asset_id ];
                entry.running_balances[ name ][ fee_asset_id ] = running_balances[ fee_asset_id ];
            }

            if( account_specified )
            {
                /* Don't return fees we didn't pay */
                if( trx.is_virtual || ( !any_from_me && !trx.is_market_cancel ) )
                {
                    trx.fee = asset();
                }
            }
        }
    }

    return pretties;
} FC_CAPTURE_AND_RETHROW() }

void wallet::remove_transaction_record( const string& record_id )
{
    const auto& records = my->_wallet_db.get_transactions();
    for( const auto& record : records )
    {
       if( string( record.first ).find( record_id ) == 0 )
       {
           my->_wallet_db.remove_transaction( record.first );
           return;
       }
    }
}

pretty_transaction wallet::to_pretty_trx( const wallet_transaction_record& trx_rec ) const
{
   pretty_transaction pretty_trx;

   pretty_trx.is_virtual = trx_rec.is_virtual;
   pretty_trx.is_confirmed = trx_rec.is_confirmed;
   pretty_trx.is_market = trx_rec.is_market;
   pretty_trx.is_market_cancel = !trx_rec.is_virtual && trx_rec.is_market && trx_rec.trx.is_cancel();
   pretty_trx.trx_id = !trx_rec.is_virtual ? trx_rec.trx.id() : trx_rec.record_id;
   pretty_trx.block_num = trx_rec.block_num;

   for( const auto& entry : trx_rec.ledger_entries )
   {
       auto pretty_entry = pretty_ledger_entry();

       if( entry.from_account.valid() )
       {
           pretty_entry.from_account = get_key_label( *entry.from_account );
           if( entry.memo_from_account.valid() )
               pretty_entry.from_account += " as " + get_key_label( *entry.memo_from_account );
       }
       else if( trx_rec.is_virtual && trx_rec.block_num <= 0 )
          pretty_entry.from_account = "GENESIS";
       else if( trx_rec.is_market )
          pretty_entry.from_account = "MARKET";
       else
          pretty_entry.from_account = "UNKNOWN";

       if( entry.to_account.valid() )
          pretty_entry.to_account = get_key_label( *entry.to_account );
       else if( trx_rec.is_market )
          pretty_entry.to_account = "MARKET";
       else
          pretty_entry.to_account = "UNKNOWN";

       /* To fix running balance calculation when withdrawing delegate pay */
       if( pretty_entry.from_account == pretty_entry.to_account )
       {
          if( entry.memo.find( "withdraw pay" ) == 0 )
              pretty_entry.from_account = "NETWORK";
       }

       /* Fix labels for yield payments */
       if( entry.memo.find( "yield" ) == 0 )
       {
          pretty_entry.from_account = "NETWORK";

          if( entry.to_account )
          {
             const auto key_record = my->_wallet_db.lookup_key( *entry.to_account );
             if( key_record.valid() )
             {
                 const auto account_record = my->_wallet_db.lookup_account( key_record->account_address );
                 if( account_record.valid() )
                   pretty_entry.to_account = account_record->name;
             }
          }
       }
       else if( entry.memo.find( "burn" ) == 0 )
       {
          pretty_entry.to_account = "NETWORK";
       }
       else if( entry.memo.find( "collect vested" ) == 0 )
       {
          pretty_entry.from_account = "SHAREDROP";
       }

       /* I'm sorry - Vikram */
       /* You better be. - Dan */
       if( pretty_entry.from_account.find( "SHORT" ) == 0
           && pretty_entry.to_account.find( "SHORT" ) == 0 )
           pretty_entry.to_account.replace(0, 5, "MARGIN" );

       if( pretty_entry.from_account.find( "MARKET" ) == 0
           && pretty_entry.to_account.find( "SHORT" ) == 0 )
           pretty_entry.to_account.replace(0, 5, "MARGIN" );

       if( pretty_entry.from_account.find( "SHORT" ) == 0
           && pretty_entry.to_account.find( "MARKET" ) == 0 )
           pretty_entry.from_account.replace(0, 5, "MARGIN" );

       if( pretty_entry.to_account.find( "SHORT" ) == 0
           && entry.memo.find( "payoff" ) == 0 )
           pretty_entry.to_account.replace(0, 5, "MARGIN" );

       if( pretty_entry.from_account.find( "SHORT" ) == 0
           && entry.memo.find( "cover" ) == 0 )
           pretty_entry.from_account.replace(0, 5, "MARGIN" );

       pretty_entry.amount = entry.amount;
       pretty_entry.memo = entry.memo;

       pretty_trx.ledger_entries.push_back( pretty_entry );
   }

   if( !pretty_trx.is_virtual && !pretty_trx.is_market )
   {
       uint16_t unknown_count = 0;
       uint16_t from_name_count = 0;
       string from_name;
       for( const pretty_ledger_entry& entry : pretty_trx.ledger_entries )
       {
           if( entry.from_account == "UNKNOWN" )
           {
               ++unknown_count;
           }
           else if( chain_interface::is_valid_account_name( entry.from_account ) )
           {
               ++from_name_count;
               if( !from_name.empty() && entry.from_account != from_name )
               {
                   from_name_count = 0;
                   break;
               }
               from_name = entry.from_account;
           }
       }

       if( from_name_count > 0 && unknown_count > 0 && from_name_count + unknown_count == pretty_trx.ledger_entries.size() )
       {
           for( pretty_ledger_entry& entry : pretty_trx.ledger_entries )
               entry.from_account = from_name;
       }
   }

   pretty_trx.fee = trx_rec.fee;
   pretty_trx.timestamp = std::min<time_point_sec>( trx_rec.created_time, trx_rec.received_time );
   pretty_trx.expiration_timestamp = trx_rec.trx.expiration;

   return pretty_trx;
}

wallet_transaction_record wallet::get_transaction( const string& transaction_id_prefix )const
{
    FC_ASSERT( is_open() );

    if( transaction_id_prefix.size() > string( transaction_id_type() ).size() )
        FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction ID!", ("transaction_id_prefix",transaction_id_prefix) );

    const auto& items = my->_wallet_db.get_transactions();
    for( const auto& item : items )
    {
        if( string( item.first ).find( transaction_id_prefix ) == 0 )
            return item.second;
    }

    FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );
}

account_balance_summary_type wallet::compute_historic_balance( const string &account_name,
                                                                  uint32_t block_num )const
{ try {
    const vector<pretty_transaction> ledger = get_pretty_transaction_history( account_name,
                                                                              0, block_num,
                                                                              "" );
    map<string, map<asset_id_type, share_type>> balances;

    for( const auto& trx : ledger )
    {
        for( const auto& entry : trx.ledger_entries )
        {
            for( const auto &account_balances : entry.running_balances )
            {
                const string name = account_balances.first;
                for( const auto &balance : account_balances.second )
                {
                    if( balance.second.amount == 0 )
                    {
                        balances[name].erase(balance.first);
                    } else {
                        balances[name][balance.first] = balance.second.amount;
                    }
                }
            }
        }
    }

    return balances;
} FC_CAPTURE_AND_RETHROW( (account_name) (block_num) ) }
