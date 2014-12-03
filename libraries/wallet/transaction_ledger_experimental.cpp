#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/time.hpp>

// TODO: Temporary
#include <fc/io/json.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

// TODO: Handle vesting sharedrop balances
void wallet_impl::scan_genesis_experimental( const account_balance_record_summary_type& account_balances )
{ try {
    transaction_ledger_entry record;
    record.id = fc::ripemd160::hash( string( "GENESIS" ) );
    record.block_num = 0;
    record.timestamp = _blockchain->get_genesis_timestamp();

    for( const auto& item : account_balances )
    {
        const string& account_name = item.first;
        for( const auto& balance_record : item.second )
        {
            if( !balance_record.snapshot_info.valid() )
                continue;

            const string& claim_addr = balance_record.snapshot_info->original_address;
            const asset delta_amount = asset( balance_record.snapshot_info->original_balance, balance_record.condition.asset_id );
            record.delta_amounts[ claim_addr ][ delta_amount.asset_id ] -= delta_amount.amount;
            record.delta_amounts[ account_name ][ delta_amount.asset_id ] += delta_amount.amount;
        }
    }

    record.operation_notes[ 0 ] = "import snapshot keys";

    _wallet_db.experimental_transactions[ record.id ] = record;
} FC_CAPTURE_AND_RETHROW( (account_balances) ) }

void wallet_impl::scan_block_experimental( uint32_t block_num,
                                           const map<private_key_type, string>& account_keys,
                                           const map<address, string>& account_balances,
                                           const set<string>& account_names )
{ try {
    const signed_block_header block_header = _blockchain->get_block_header( block_num );
    const vector<transaction_record> transaction_records = _blockchain->get_transactions_for_block( block_header.id() );
    for( const transaction_evaluation_state& eval_state : transaction_records )
    {
        scan_transaction_experimental( eval_state, block_num, block_header.timestamp,
                                       account_keys, account_balances, account_names, true );
    }
} FC_CAPTURE_AND_RETHROW( (block_num)(account_balances)(account_names) ) }

transaction_ledger_entry wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                                     uint32_t block_num,
                                                                     const time_point_sec& timestamp,
                                                                     bool overwrite_existing )
{ try {
    const map<private_key_type, string> account_keys = _wallet_db.get_account_private_keys( _wallet_password );

    map<address, string> account_balances;
    const account_balance_id_summary_type balance_id_summary = self->get_account_balance_ids();
    for( const auto& balance_item : balance_id_summary )
    {
        const string& account_name = balance_item.first;
        for( const auto& balance_id : balance_item.second )
            account_balances[ balance_id ] = account_name;
    }

    set<string> account_names;
    const vector<wallet_account_record> accounts = self->list_my_accounts();
    for( const wallet_account_record& account : accounts )
        account_names.insert( account.name );

    return scan_transaction_experimental( eval_state, block_num, timestamp, account_keys,
                                          account_balances, account_names, overwrite_existing );
} FC_CAPTURE_AND_RETHROW( (eval_state)(block_num)(timestamp)(overwrite_existing) ) }

transaction_ledger_entry wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                                     uint32_t block_num,
                                                                     const time_point_sec& timestamp,
                                                                     const map<private_key_type, string>& account_keys,
                                                                     const map<address, string>& account_balances,
                                                                     const set<string>& account_names,
                                                                     bool overwrite_existing )
{ try {
    transaction_ledger_entry record;

    const transaction_id_type transaction_id = eval_state.trx.id();
    const transaction_id_type& record_id = transaction_id;
    const bool existing_record = _wallet_db.experimental_transactions.count( record_id ) > 0;
    if( existing_record )
        record = _wallet_db.experimental_transactions.at( record_id );

    record.id = record_id;
    record.block_num = block_num;
    record.timestamp = std::min( record.timestamp, timestamp );
    record.delta_amounts.clear();
    record.transaction_id = transaction_id;

    scan_transaction_experimental( eval_state, account_keys, account_balances, account_names, record,
                                   overwrite_existing || !existing_record );

    return record;
} FC_CAPTURE_AND_RETHROW( (eval_state)(block_num)(timestamp)(account_balances)(account_names)(overwrite_existing) ) }

void wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                 const map<private_key_type, string>& account_keys,
                                                 const map<address, string>& account_balances,
                                                 const set<string>& account_names,
                                                 transaction_ledger_entry& record,
                                                 bool store_record )
{ try {
    uint16_t op_index = 0;
    uint16_t withdrawal_count = 0;
    vector<memo_status> titan_memos;

    const auto collect_balance = [&]( const balance_id_type& balance_id, const asset& delta_amount ) -> bool
    {
        if( account_balances.count( balance_id ) > 0 )
        {
            const string& delta_label = account_balances.at( balance_id );
            // TODO: Need to save balance labels locally before emptying them so they can be deleted from the chain
            record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return true;
        }
        else if( record.delta_labels.count( op_index ) > 0 )
        {
            const string& delta_label = record.delta_labels.at( op_index );
            record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return account_names.count( delta_label ) > 0;
        }
        else
        {
            const string delta_label = string( balance_id );
            record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        }
    };

    const auto scan_withdraw = [&]( const withdraw_operation& op ) -> bool
    {
        // TODO: If withdrawing vesting balance, then set delta label appropriately and override account name
        ++withdrawal_count;
        return collect_balance( op.balance_id, eval_state.deltas.at( op_index ) );
    };

    // TODO: Recipient address label and memo message needs to be saved at time of creation by sender
    // to do this make a wrapper around trx.deposit_to_account just like my->withdraw_to_transaction
    const auto scan_deposit = [&]( const deposit_operation& op ) -> bool
    {
        const balance_id_type balance_id = op.balance_id();
        const asset& delta_amount = eval_state.deltas.at( op_index );

        const auto scan_withdraw_with_signature = [&]( const withdraw_with_signature& condition ) -> bool
        {
            if( condition.memo.valid() ) // titan
            {
                if( record.delta_labels.count( op_index ) == 0 )
                {
                    vector<fc::future<void>> decrypt_memo_futures;
                    decrypt_memo_futures.resize( account_keys.size() );
                    uint32_t key_index = 0;

                    for( const auto& key_item : account_keys )
                    {
                        const private_key_type& key = key_item.first;
                        const string& account_name = key_item.second;

                        decrypt_memo_futures[ key_index ] = fc::async( [&, key_index]()
                        {
                            omemo_status status;
                            _scanner_threads[ key_index % _num_scanner_threads ]->async( [&]()
                            {
                                status = condition.decrypt_memo_data( key );
                            }, "decrypt memo" ).wait();

                            if( status.valid() )
                            {
                                _wallet_db.cache_memo( *status, key, _wallet_password );

                                titan_memos.push_back( *status );

                                const string& delta_label = account_name;
                                record.delta_labels[ op_index ] = delta_label;

                                const string memo = status->get_message();
                                if( !memo.empty() )
                                    record.operation_notes[ op_index ] = memo;
                            }
                        } );

                        ++key_index;
                    }

                    for( auto& decrypt_memo_future : decrypt_memo_futures )
                    {
                        try
                        {
                            decrypt_memo_future.wait();
                        }
                        catch( const fc::exception& e )
                        {
                            elog( "unexpected exception waiting on memo decryption task ${e}", ("e",e.to_detail_string()) );
                        }
                    }
                }
            }

            return collect_balance( balance_id, delta_amount );
        };

        switch( withdraw_condition_types( op.condition.type ) )
        {
            case withdraw_signature_type:
                return scan_withdraw_with_signature( op.condition.as<withdraw_with_signature>() );
            default:
                break;
        }

        return false;
    };

    const auto scan_register_account = [&]( const register_account_operation& op ) -> bool
    {
        const string& account_name = op.name;

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "register account " + account_name;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_update_account = [&]( const update_account_operation& op ) -> bool
    {
        const oaccount_record account_record = _blockchain->get_account_record( op.account_id );
        FC_ASSERT( account_record.valid() );
        const string& account_name = account_record->name;

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "update account " + account_name;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_withdraw_pay = [&]( const withdraw_pay_operation& op ) -> bool
    {
        const oaccount_record account_record = _blockchain->get_account_record( op.account_id );
        FC_ASSERT( account_record.valid() );
        const string& account_name = account_record->name;

        const string delta_label = "INCOME-" + account_name;
        const asset& delta_amount = eval_state.deltas.at( op_index );
        record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

        return account_names.count( account_name ) > 0;
    };

    const auto scan_create_asset = [&]( const create_asset_operation& op ) -> bool
    {
        // TODO: Issuer could be asset_record::market_issued_asset
        const oaccount_record account_record = _blockchain->get_account_record( op.issuer_account_id );
        FC_ASSERT( account_record.valid() );
        const string& account_name = account_record->name;

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = account_name + " created asset " + op.symbol;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_issue_asset = [&]( const issue_asset_operation& op ) -> bool
    {
        const asset& delta_amount = eval_state.deltas.at( op_index );
        const oasset_record asset_record = _blockchain->get_asset_record( delta_amount.asset_id );
        FC_ASSERT( asset_record.valid() );

        const oaccount_record account_record = _blockchain->get_account_record( asset_record->issuer_account_id );
        FC_ASSERT( account_record.valid() );
        const string& account_name = account_record->name;

        const string delta_label = "ISSUER-" + account_name;
        record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

        return account_names.count( account_name ) > 0;
    };

    const auto scan_ask = [&]( const ask_operation& op ) -> bool
    {
        const market_order order( ask_order, op.ask_index, op.amount );
        const string delta_label = order.get_small_id();
        const asset& delta_amount = eval_state.deltas.at( op_index );
        record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

        if( record.operation_notes.count( op_index ) == 0 )
        {
            string note;

            if( op.amount >= 0 )
            {
                const oasset_record asset_record = _blockchain->get_asset_record( op.ask_index.order_price.base_asset_id );
                FC_ASSERT( asset_record.valid() );
                note = "sell " + asset_record->symbol + " @ " + _blockchain->to_pretty_price( op.ask_index.order_price );
            }
            else
            {
                note = "cancel " + delta_label;
            }

            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record key_record = _wallet_db.lookup_key( op.ask_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_update_feed = [&]( const update_feed_operation& op ) -> bool
    {
        const oasset_record asset_record = _blockchain->get_asset_record( op.feed.feed_id );
        FC_ASSERT( asset_record.valid() );

        const oaccount_record account_record = _blockchain->get_account_record( op.feed.delegate_id );
        FC_ASSERT( account_record.valid() );
        const string& account_name = account_record->name;

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "update " + account_name + "'s price feed for " + asset_record->symbol;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_burn = [&]( const burn_operation& op ) -> bool
    {
        const string delta_label = "INCINERATOR";
        const asset& delta_amount = op.amount;
        record.delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

        const account_id_type& account_id = op.account_id;
        const oaccount_record account_record = _blockchain->get_account_record( abs( account_id ) );

        if( account_record.valid() )
        {
            const string& account_name = account_record->name;

            if( record.operation_notes.count( op_index ) == 0 )
            {
                string note = "burn";
                note += ( account_id > 0 ) ? " for " : " against ";
                note += account_name;
                record.operation_notes[ op_index ] = note;
            }

            return account_names.count( account_name ) > 0;
        }

        return false;
    };

    bool my_transaction = false;
    for( const auto& op : eval_state.trx.operations )
    {
        switch( operation_type_enum( op.type ) )
        {
            case withdraw_op_type:
                my_transaction |= scan_withdraw( op.as<withdraw_operation>() );
                break;
            case deposit_op_type:
                my_transaction |= scan_deposit( op.as<deposit_operation>() );
                break;
            case register_account_op_type:
                my_transaction |= scan_register_account( op.as<register_account_operation>() );
                break;
            case update_account_op_type:
                my_transaction |= scan_update_account( op.as<update_account_operation>() );
                break;
            case withdraw_pay_op_type:
                my_transaction |= scan_withdraw_pay( op.as<withdraw_pay_operation>() );
                break;
            case create_asset_op_type:
                my_transaction |= scan_create_asset( op.as<create_asset_operation>() );
                break;
            case update_asset_op_type:
                // Not yet exposed to users
                break;
            case issue_asset_op_type:
                my_transaction |= scan_issue_asset( op.as<issue_asset_operation>() );
                break;
            case bid_op_type:
                // TODO
                break;
            case ask_op_type:
                my_transaction |= scan_ask( op.as<ask_operation>() );
                break;
            case short_op_type:
                // TODO
                break;
            case cover_op_type:
                // TODO
                break;
            case define_delegate_slate_op_type:
                // Don't care; do nothing
                break;
            case update_feed_op_type:
                my_transaction |= scan_update_feed( op.as<update_feed_operation>() );
                break;
            case burn_op_type:
                my_transaction |= scan_burn( op.as<burn_operation>() );
                break;
            default:
                break;
        }

        ++op_index;
    }

    // Kludge to build pretty incoming TITAN transfers
    // We only support such transfers when all operations are withdrawals plus a single deposit
    const auto rescan_with_titan_info = [&]() -> bool
    {
        if( withdrawal_count + titan_memos.size() != eval_state.trx.operations.size() )
            return false;

        if( titan_memos.size() != 1 )
            return false;

        if( record.delta_labels.size() != 1 )
            return false;

        const memo_status& memo = titan_memos.front();
        if( !memo.has_valid_signature )
            return false;

        string account_name;
        const address from_address( memo.from );
        const oaccount_record chain_account_record = _blockchain->get_account_record( from_address );
        const owallet_account_record local_account_record = _wallet_db.lookup_account( from_address );
        if( chain_account_record.valid() )
            account_name = chain_account_record->name;
        else if( local_account_record.valid() )
            account_name = local_account_record->name;
        else
            return false;

        record.delta_amounts.clear();

        for( uint16_t i = 0; i < eval_state.trx.operations.size(); ++i )
        {
            if( operation_type_enum( eval_state.trx.operations.at( i ).type ) == withdraw_op_type )
                record.delta_labels[ i ] = account_name;
        }

        scan_transaction_experimental( eval_state, account_keys, account_balances, account_names, record, store_record );
        return true;
    };

    if( rescan_with_titan_info() )
        return;

    for( const auto& delta_item : eval_state.balance )
    {
        const asset delta_amount( delta_item.second, delta_item.first );
        record.delta_amounts[ "FEE" ][ delta_amount.asset_id ] += delta_amount.amount;
    }

    if( my_transaction && store_record )
    {
        ulog( "wallet_transaction_record_v2:\n${rec}", ("rec",fc::json::to_pretty_string( record )) );
        _wallet_db.experimental_transactions[ record.id ] = record;
    }
} FC_CAPTURE_AND_RETHROW( (eval_state)(account_balances)(account_names)(record)(store_record) ) }

transaction_ledger_entry wallet_impl::apply_transaction_experimental( const signed_transaction& transaction )
{ try {
   const transaction_evaluation_state_ptr eval_state = _blockchain->store_pending_transaction( transaction, true );
   FC_ASSERT( eval_state != nullptr );
   return scan_transaction_experimental( *eval_state, -1, blockchain::now(), true );
} FC_CAPTURE_AND_RETHROW( (transaction) ) }

transaction_ledger_entry wallet::scan_transaction_experimental( const string& transaction_id_prefix, bool overwrite_existing )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   // TODO: Separate this finding logic
   if( transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string( transaction_id_type() ).size() )
       FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix",transaction_id_prefix) );

   const auto transaction_id = variant( transaction_id_prefix ).as<transaction_id_type>();
   const auto transaction_record = my->_blockchain->get_transaction( transaction_id, false );
   if( !transaction_record.valid() )
       FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );

   const auto block_num = transaction_record->chain_location.block_num;
   const auto block = my->_blockchain->get_block_header( block_num );

   return my->scan_transaction_experimental( *transaction_record, block_num, block.timestamp, overwrite_existing );
} FC_CAPTURE_AND_RETHROW( (transaction_id_prefix)(overwrite_existing) ) }

void wallet::add_transaction_note_experimental( const string& transaction_id_prefix, const string& note )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   // TODO: Separate this finding logic
   if( transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string( transaction_id_type() ).size() )
       FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix",transaction_id_prefix) );

   const auto transaction_id = variant( transaction_id_prefix ).as<transaction_id_type>();
   const auto transaction_record = my->_blockchain->get_transaction( transaction_id, false );
   if( !transaction_record.valid() )
       FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );

   const auto record_id = transaction_record->trx.id();
   if( my->_wallet_db.experimental_transactions.count( record_id ) == 0 )
       FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );

   auto record = my->_wallet_db.experimental_transactions[ record_id ];
   if( !note.empty() )
       record.operation_notes[ transaction_record->trx.operations.size() ] = note;
   else
       record.operation_notes.erase( transaction_record->trx.operations.size() );
   my->_wallet_db.experimental_transactions[ record_id ] = record;

} FC_CAPTURE_AND_RETHROW( (transaction_id_prefix)(note) ) }

set<pretty_transaction_experimental> wallet::transaction_history_experimental( const string& account_name )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   my->scan_genesis_experimental( get_account_balance_records() );

   set<pretty_transaction_experimental> history;

   const auto& transactions = my->_wallet_db.get_transactions();
   for( const auto& item : transactions )
   {
       try
       {
           scan_transaction_experimental( string( item.second.trx.id() ), false );
       }
       catch( ... )
       {
       }
   }

   for( const auto& item : my->_wallet_db.experimental_transactions )
   {
       history.insert( to_pretty_transaction_experimental( item.second ) );
   }

   const auto account_records = list_my_accounts();
   // TODO: Merge this into to_pretty_trx
   map<string, map<asset_id_type, share_type>> balances;
   for( const auto& record : history )
   {
       for( const auto& item : record.delta_amounts )
       {
           const string& label = item.first;
           const auto has_label = [&]( const wallet_account_record& account_record )
           {
               return account_record.name == label;
           };

           if( !std::any_of( account_records.begin(), account_records.end(), has_label ) )
               continue;

           for( const auto& delta_item : item.second )
           {
               const asset delta_amount( delta_item.second, delta_item.first );
               balances[ label ][ delta_amount.asset_id ] += delta_amount.amount;
           }
       }

       for( const auto& item : balances )
       {
           const string& label = item.first;
           for( const auto& balance_item : item.second )
           {
               const asset balance_amount( balance_item.second, balance_item.first );
               record.balances.push_back( std::make_pair( label, balance_amount ) );
           }
       }
   }

   return history;
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

pretty_transaction_experimental wallet::to_pretty_transaction_experimental( const transaction_ledger_entry& record )
{ try {
   pretty_transaction_experimental result;
   transaction_ledger_entry& ugly_result = result;
   ugly_result = record;

   for( const auto& item : record.delta_amounts )
   {
       const string& label = item.first;
       for( const auto& delta_item : item.second )
       {
           const asset delta_amount( delta_item.second, delta_item.first );
           if( delta_amount.amount >= 0)
               result.outputs.push_back( std::make_pair( label, delta_amount ) );
           else
               result.inputs.push_back( std::make_pair( label, -delta_amount ) );
       }
   }

   result.notes.reserve( record.operation_notes.size() );
   for( const auto& item : record.operation_notes )
   {
       const auto& note = item.second;
       result.notes.push_back( note );
   }

   const auto delta_compare = []( const std::pair<string, asset>& a, const std::pair<string, asset>& b ) -> bool
   {
       const string& a_label = a.first;
       const string& b_label = b.first;

       const asset& a_amount = a.second;
       const asset& b_amount = b.second;

       if( a_label == b_label )
           return a_amount.asset_id < b_amount.asset_id;

       if( islower( a_label[0] ) != islower( b_label[0] ) )
           return islower( a_label[0] );

       if( (a_label.find( "FEE" ) == string::npos) != (b_label.find( "FEE" ) == string::npos) )
           return a_label.find( "FEE" ) == string::npos;

       return a < b;
   };

   std::sort( result.inputs.begin(), result.inputs.end(), delta_compare );
   std::sort( result.outputs.begin(), result.outputs.end(), delta_compare );

   return result;
} FC_CAPTURE_AND_RETHROW( (record) ) }
