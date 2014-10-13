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
        const time_point_sec& block_time,
        const time_point_sec& received_time
        )
{ try {
    auto okey_bid = _wallet_db.lookup_key( mtrx.bid_owner );
    if( okey_bid && okey_bid->has_private_key() )
    {
        const auto bid_account_key = _wallet_db.lookup_key( okey_bid->account_address );

        auto bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.bid_owner),
                                                                            mtrx.bid_price.base_asset_id ).get_address() );
        if( bal_rec.valid() )
            sync_balance_with_blockchain( bal_rec->id() );

        bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.bid_owner),
                                                                      mtrx.bid_price.quote_asset_id ).get_address() );
        if( bal_rec.valid() )
            sync_balance_with_blockchain( bal_rec->id() );

        /* Construct a unique record id */
        std::stringstream id_ss;
        id_ss << block_num << string( mtrx.bid_owner ) << string( mtrx.ask_owner );

        // TODO: Don't blow away memo, etc.
        auto record = wallet_transaction_record();
        record.record_id = fc::ripemd160::hash( id_ss.str() );
        record.block_num = block_num;
        record.is_virtual = true;
        record.is_confirmed = true;
        record.is_market = true;
        record.created_time = block_time;
        record.received_time = received_time;

        if( mtrx.bid_type == bid_order )
        {
            {
                auto entry = ledger_entry();
                entry.from_account = okey_bid->public_key;
                //entry.to_account = "MARKET";
                entry.amount = mtrx.bid_paid;
                entry.memo = "pay bid @ " + _blockchain->to_pretty_price( mtrx.bid_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                entry.from_account = okey_bid->public_key;
                entry.to_account = bid_account_key->public_key;
                entry.amount = mtrx.bid_received;
                entry.memo = "bid proceeds @ " + _blockchain->to_pretty_price( mtrx.bid_price );
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
                    entry.memo = "short proceeds @ " + _blockchain->to_pretty_price( mtrx.bid_price );
                    record.ledger_entries.push_back( entry );
                    self->update_margin_position( entry );
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
    }

    auto okey_ask = _wallet_db.lookup_key( mtrx.ask_owner );
    if( okey_ask && okey_ask->has_private_key() )
    {
        const auto ask_account_key = _wallet_db.lookup_key( okey_ask->account_address );

        auto bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.ask_owner),
                                                                            mtrx.ask_price.base_asset_id ).get_address() );
        if( bal_rec.valid() )
            sync_balance_with_blockchain( bal_rec->id() );

        bal_rec = _blockchain->get_balance_record( withdraw_condition( withdraw_with_signature(mtrx.ask_owner),
                                                                      mtrx.ask_price.quote_asset_id ).get_address() );
        if( bal_rec.valid() )
            sync_balance_with_blockchain( bal_rec->id() );

        /* Construct a unique record id */
        std::stringstream id_ss;
        id_ss << block_num << string( mtrx.ask_owner ) << string( mtrx.bid_owner );

        // TODO: Don't blow away memo, etc.
        auto record = wallet_transaction_record();
        record.record_id = fc::ripemd160::hash( id_ss.str() );
        record.block_num = block_num;
        record.is_virtual = true;
        record.is_confirmed = true;
        record.is_market = true;
        record.created_time = block_time;
        record.received_time = received_time;

        if( mtrx.ask_type == ask_order )
        {
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                //entry.to_account = "MARKET";
                entry.amount = mtrx.ask_paid;
                entry.memo = "fill ask @ " + _blockchain->to_pretty_price( mtrx.ask_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                entry.from_account = okey_ask->public_key;
                entry.to_account = ask_account_key->public_key;
                entry.amount = mtrx.ask_received;
                entry.memo = "ask proceeds @ " + _blockchain->to_pretty_price( mtrx.ask_price );
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
                entry.memo = "sell collateral @ " + _blockchain->to_pretty_price( mtrx.ask_price );
                record.ledger_entries.push_back( entry );
            }
            {
                auto entry = ledger_entry();
                //entry.from_account = "MARKET";
                entry.to_account = okey_ask->public_key;
                entry.amount = mtrx.ask_received;
                entry.memo = "payoff debt @ " + _blockchain->to_pretty_price( mtrx.ask_price );
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
                record.fee = mtrx.fees_collected;
            }
        }

        _wallet_db.store_transaction( record );
    }
} FC_CAPTURE_AND_RETHROW() }

// TODO: No longer needed with scan_genesis_experimental and get_account_balance_records
void wallet_impl::scan_balances()
{
   /* Delete ledger entries for any genesis balances before we can reconstruct them */
   const auto my_accounts = self->list_my_accounts();
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
   _blockchain->scan_balances( [&]( const balance_record& bal_rec )
   {
        const auto key_rec = _wallet_db.lookup_key( bal_rec.owner() );
        if( key_rec.valid() && key_rec->has_private_key() )
        {
          sync_balance_with_blockchain( bal_rec.id() );

          if( bal_rec.genesis_info.valid() ) /* Create virtual transactions for genesis claims */
          {
              const auto public_key = key_rec->public_key;
              const auto record_id = fc::ripemd160::hash( self->get_key_label( public_key ) );
              auto transaction_record = _wallet_db.lookup_transaction( record_id );
              if( !transaction_record.valid() )
              {
                  transaction_record = wallet_transaction_record();
                  transaction_record->created_time = timestamp;
                  transaction_record->received_time = timestamp;
              }

              auto entry = ledger_entry();
              entry.to_account = public_key;
              entry.amount = bal_rec.genesis_info->initial_balance;
              entry.memo = "claim " + bal_rec.genesis_info->claim_addr;

              transaction_record->record_id = record_id;
              transaction_record->is_virtual = true;
              transaction_record->is_confirmed = true;
              transaction_record->ledger_entries.push_back( entry );
              _wallet_db.store_transaction( *transaction_record );
          }
        }
   } );
}

void wallet_impl::scan_registered_accounts()
{
   _blockchain->scan_accounts( [&]( const blockchain::account_record& scanned_account_record )
   {
        // TODO: check owner key as well!
        auto key_rec =_wallet_db.lookup_key( scanned_account_record.active_key() );
        if( key_rec.valid() && key_rec->has_private_key() )
        {
           auto existing_account_record = _wallet_db.lookup_account( key_rec->account_address );
           if( existing_account_record.valid() )
           {
              blockchain::account_record& as_blockchain_account_record = *existing_account_record;
              as_blockchain_account_record = scanned_account_record;
              _wallet_db.cache_account( *existing_account_record );
           }
        }
   } );
   ilog( "account scan complete" );
}

void wallet_impl::scan_block( uint32_t block_num, const vector<private_key_type>& keys, const time_point_sec& received_time )
{
   const auto block = _blockchain->get_block( block_num );
   for( const auto& transaction : block.user_transactions )
      scan_transaction( transaction, block_num, block.timestamp, keys, received_time );

   const auto market_trxs = _blockchain->get_market_transactions( block_num );
   for( const auto& market_trx : market_trxs )
      scan_market_transaction( market_trx, block_num, block.timestamp, received_time );
}

wallet_transaction_record wallet_impl::scan_transaction(
        const signed_transaction& transaction,
        uint32_t block_num,
        const time_point_sec& block_timestamp,
        const vector<private_key_type>& keys,
        const time_point_sec& received_time,
        bool overwrite_existing )
{ try {
    const auto record_id = transaction.id();
    auto transaction_record = _wallet_db.lookup_transaction( record_id );
    const auto already_exists = transaction_record.valid();
    if( !already_exists )
    {
        transaction_record = wallet_transaction_record();
        transaction_record->record_id = record_id;
        transaction_record->created_time = block_timestamp;
        transaction_record->received_time = received_time;
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
            case short_op_type:
            {
                const auto short_op = op.as<short_operation>();
                if( short_op.amount < 0 )
                    has_withdrawal |= scan_short( short_op, *transaction_record, total_fee );
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
                is_deposit = scan_deposit( op.as<deposit_operation>(), keys, *transaction_record, total_fee );
                has_deposit |= is_deposit;
                break;
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
            case short_op_type:
            {
                const auto short_op = op.as<short_operation>();
                if( short_op.amount >= 0 )
                    has_deposit |= scan_short( short_op, *transaction_record, total_fee );
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
            case update_asset_op_type:
                // TODO
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
       auto blockchain_trx_state = _blockchain->get_transaction( record_id );
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

              for( const auto& yield_item : blockchain_trx_state->yield )
              {
                 auto entry = ledger_entry();
                 entry.amount = asset( yield_item.second, yield_item.first );
                 entry.to_account = withdraw_pub_key;
                 entry.from_account = withdraw_pub_key;
                 entry.memo = "yield";
                 transaction_record->ledger_entries.push_back( entry );
                 self->wallet_claimed_transaction( transaction_record->ledger_entries.back() );
              }

              if( !blockchain_trx_state->yield.empty() )
                 _wallet_db.store_transaction( *transaction_record );
          }
       }
    }

    /* Only overwrite existing record if you did not create it or overwriting was explicitly specified */
    if( store_record && ( !already_exists || overwrite_existing ) )
        _wallet_db.store_transaction( *transaction_record );

    return *transaction_record;
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

   // TODO: Only if withdraw by signature or by name
   const auto key_rec =_wallet_db.lookup_key( bal_rec->owner() );
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

       sync_balance_with_blockchain( op.balance_id );
       return true;
   }
   return false;
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

    blockchain::account_record& as_blockchain_account_record = *opt_account;
    as_blockchain_account_record = *account_name_rec;
    _wallet_db.cache_account( *opt_account );

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

    blockchain::account_record& as_blockchain_account_record = *opt_account;
    as_blockchain_account_record = *account_name_rec;
    _wallet_db.cache_account( *opt_account );

    if( !opt_account->is_my_account )
      return false;

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

    return true;
} FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

bool wallet_impl::scan_create_asset( const create_asset_operation& op, wallet_transaction_record& trx_rec )
{
   if( op.issuer_account_id != asset_record::market_issued_asset )
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

// TODO: optimize
bool wallet_impl::scan_deposit( const deposit_operation& op, const vector<private_key_type>& keys,
                                wallet_transaction_record& trx_rec, asset& total_fee )
{ try {
    auto amount = asset( op.amount, op.condition.asset_id );
    if( amount.asset_id == total_fee.asset_id )
        total_fee -= amount;

    bool cache_deposit = false;
    switch( (withdraw_condition_types) op.condition.type )
    {
       case withdraw_null_type:
       {
          FC_THROW( "withdraw_null_type not implemented!" );
          break;
       }
       case withdraw_signature_type:
       {
          auto deposit = op.condition.as<withdraw_with_signature>();
          // TODO: lookup if cached key and work with it only
          // if( _wallet_db.has_private_key( deposit.owner ) )
          if( deposit.memo ) /* titan transfer */
          {
             vector< fc::future<void> > scan_key_progress;
             scan_key_progress.resize( keys.size() );
             for( uint32_t i = 0; i < keys.size(); ++i )
             {
                const auto& key = keys[i];
                scan_key_progress[i] = fc::async([&,i](){
                   omemo_status status;
                   _scanner_threads[ i % _num_scanner_threads ]->async( [&]()
                       { status =  deposit.decrypt_memo_data( key ); }, "decrypt memo" ).wait();
                   if( status.valid() ) /* If I've successfully decrypted then it's for me */
                   {
                      cache_deposit = true;
                      _wallet_db.cache_memo( *status, key, _wallet_password );

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
                             entry.to_account = key.get_public_key();
                             entry.amount = amount;
                             entry.memo = status->get_message();
                             break;
                         }
                         if( new_entry )
                         {
                             auto entry = ledger_entry();
                             entry.from_account = status->from;
                             entry.to_account = key.get_public_key();
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
                             const auto a2 = self->get_key_label( key.get_public_key() );
                             if( a1 != a2 ) continue;

                             new_entry = false;
                             entry.from_account = key.get_public_key();
                             entry.to_account = status->from;
                             entry.amount = amount;
                             entry.memo = status->get_message();
                             break;
                         }
                         if( new_entry )
                         {
                             auto entry = ledger_entry();
                             entry.from_account = key.get_public_key();
                             entry.to_account = status->from;
                             entry.amount = amount;
                             entry.memo = status->get_message();
                             trx_rec.ledger_entries.push_back( entry );
                         }
                      }
                   }
               });
             } // for each key

             for( auto& fut : scan_key_progress )
             {
                try {
                   fut.wait();
                }
                catch ( const fc::exception& e )
                {
                   elog( "unexpected exception ${e}", ("e",e.to_detail_string()) );
                }
             }
             break;
          }
          else /* market cancel or cover proceeds */
          {
             const auto okey_rec = _wallet_db.lookup_key( deposit.owner );
             if( okey_rec && okey_rec->has_private_key() )
             {
                 cache_deposit = true;
                 for( auto& entry : trx_rec.ledger_entries )
                 {
                     if( !entry.from_account.valid() ) continue;
                     const auto account_rec = self->get_account_for_address( okey_rec->public_key );
                     if( !account_rec.valid() ) continue;
                     const auto account_key_rec = _wallet_db.lookup_key( account_rec->account_address );
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
                     break;
                 }
             }
          }
          break;
       }
       case withdraw_multi_sig_type:
       {
          // TODO: FC_THROW( "withdraw_multi_sig_type not implemented!" );
          break;
       }
       case withdraw_password_type:
       {
          // TODO: FC_THROW( "withdraw_password_type not implemented!" );
          break;
       }
       case withdraw_option_type:
       {
          // TODO: FC_THROW( "withdraw_option_type not implemented!" );
          break;
       }
       default:
       {
          FC_THROW( "unknown withdraw condition type!" );
          break;
       }
  }

  if( cache_deposit )
      sync_balance_with_blockchain( op.balance_id() );

  return cache_deposit;
} FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) } // wallet_impl::scan_deposit

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

   const auto block_num = transaction_record->chain_location.block_num;
   const auto block = my->_blockchain->get_block_header( block_num );
   const auto keys = my->_wallet_db.get_account_private_keys( my->_wallet_password );
   vector<private_key_type> private_keys;
   private_keys.reserve( keys.size() );
   for( const auto& item : keys )
       private_keys.push_back( item.first );
   const auto now = blockchain::now();
   return my->scan_transaction( transaction_record->trx, block_num, block.timestamp, private_keys, now, overwrite_existing );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

void wallet::sign_transaction( signed_transaction& transaction, const unordered_set<address>& required_signatures )const
{ try {
   transaction.expiration = blockchain::now() + get_transaction_expiration();
   const auto chain_id = my->_blockchain->chain_id();
   for( const auto& addr : required_signatures )
       transaction.sign( get_private_key( addr ), chain_id );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

void wallet::cache_transaction( const signed_transaction& transaction, wallet_transaction_record& record, bool apply_transaction )
{ try {
   if( apply_transaction ) // Should only be false when apply_transaction_experimental is used
       my->_blockchain->store_pending_transaction( transaction, true );

   record.record_id = transaction.id();
   record.trx = transaction;
   record.created_time = blockchain::now();
   record.received_time = record.created_time;
   my->_wallet_db.store_transaction( record );

   for( const auto& op : transaction.operations )
   {
       if( operation_type_enum( op.type ) == withdraw_op_type )
           my->sync_balance_with_blockchain( op.as<withdraw_operation>().balance_id );
   }
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

   auto asset_id = 0;
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
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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
        const auto accounts = list_my_accounts();
        for( const auto& account : accounts )
            account_names.push_back( account.name );
    }
    else
    {
        account_names.push_back( account_name );
    }

    /* Tally up running balances */
    for( const auto& name : account_names )
    {
        map<asset_id_type, asset> running_balances;
        for( auto& trx : pretties )
        {
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
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

wallet_transaction_record wallet::edit_transaction( const string& transaction_id_prefix, const string& recipient_account,
                                                    const string& memo_message )
{
    FC_ASSERT( !recipient_account.empty() || !memo_message.empty() );
    auto transaction_record = get_transaction( transaction_id_prefix );

    /* Only support standard transfers for now */
    FC_ASSERT( transaction_record.ledger_entries.size() == 1 );
    auto ledger_entry = transaction_record.ledger_entries.front();

    if( !recipient_account.empty() )
        ledger_entry.to_account = get_account_public_key( recipient_account );

    if( !memo_message.empty() )
        ledger_entry.memo = memo_message;

    transaction_record.ledger_entries[ 0 ] = ledger_entry;
    my->_wallet_db.store_transaction( transaction_record );

    return transaction_record;
}

pretty_transaction wallet::to_pretty_trx( const wallet_transaction_record& trx_rec ) const
{
   pretty_transaction pretty_trx;

   pretty_trx.is_virtual = trx_rec.is_virtual;
   pretty_trx.is_confirmed = trx_rec.is_confirmed;
   pretty_trx.is_market = trx_rec.is_market;
   pretty_trx.is_market_cancel = !trx_rec.is_virtual && trx_rec.is_market && trx_rec.trx.is_cancel();
   pretty_trx.trx_id = trx_rec.record_id;
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
          pretty_entry.to_account = "NETWORK";

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
