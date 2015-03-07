#include <bts/blockchain/time.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <fc/io/json.hpp> // TODO: Temporary

namespace bts { namespace wallet {

void detail::wallet_impl::scan_balances_experimental()
{ try {
    const auto pending_state = _blockchain->get_pending_state();

    const auto scan_balance = [&]( const balance_record& record )
    {
        const balance_id_type id = record.id();
        const obalance_record pending_record = pending_state->get_balance_record( id );
        if( !pending_record.valid() ) return;

        const set<address>& owners = pending_record->owners();
        for( const address& owner : owners )
        {
            const owallet_key_record key_record = _wallet_db.lookup_key( owner );
            if( !key_record.valid() || !key_record->has_private_key() ) continue;

            _balance_records[ id ] = std::move( *pending_record );
            break;
        }
    };

    _balance_records.clear();
    _blockchain->scan_balances( scan_balance );
    _dirty_balances = false;
} FC_CAPTURE_AND_RETHROW() }

void detail::wallet_impl::scan_accounts()
{ try {
    const auto check_address = [&]( const address& addr ) -> bool
    {
        if( _wallet_db.lookup_account( addr ).valid() ) return true;
        const owallet_key_record& key_record = _wallet_db.lookup_key( addr );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_account = [&]( const account_record& blockchain_record )
    {
        bool store_account = check_address( blockchain_record.owner_address() );
        if( !store_account ) store_account = check_address( blockchain_record.active_address() );
        if( !store_account && blockchain_record.is_delegate() ) store_account = check_address( blockchain_record.signing_address() );
        if( !store_account ) return;
        _wallet_db.store_account( blockchain_record );
    };

    _blockchain->scan_unordered_accounts( scan_account );

    if( self->is_unlocked() )
    {
        _stealth_private_keys.clear();
        const auto& account_keys = _wallet_db.get_account_private_keys( _wallet_password );
        _stealth_private_keys.reserve( account_keys.size() );
        for( const auto& item : account_keys )
           _stealth_private_keys.push_back( item.first );
    }

    _dirty_accounts = false;
} FC_CAPTURE_AND_RETHROW() }

void detail::wallet_impl::scan_block_experimental( uint32_t block_num,
                                                   const map<private_key_type, string>& account_keys,
                                                   const map<address, string>& account_balances,
                                                   const set<string>& account_names )
{ try {
    const signed_block_header block_header = _blockchain->get_block_header( block_num );
    const vector<transaction_record> transaction_records = _blockchain->get_transactions_for_block( block_header.id() );
    for( const transaction_evaluation_state& eval_state : transaction_records )
    {
        try
        {
            scan_transaction_experimental( eval_state, block_num, block_header.timestamp,
                                           account_keys, account_balances, account_names, true );
        }
        catch( const fc::exception& e )
        {
            elog( "Error scanning transaction ${t}: ${e}", ("t",eval_state)("e",e.to_detail_string()) );
        }
    }
} FC_CAPTURE_AND_RETHROW( (block_num)(account_balances)(account_names) ) }

// For initiating manual transaction scanning
transaction_ledger_entry detail::wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                                             uint32_t block_num,
                                                                             const time_point_sec timestamp,
                                                                             bool overwrite_existing )
{ try {
    const map<private_key_type, string> account_keys = _wallet_db.get_account_private_keys( _wallet_password );

    // TODO: Move this into a separate function
    map<address, string> account_balances;
    //const account_balance_record_summary_type balance_record_summary = self->get_account_balance_records( "", true, -1 );
    const account_balance_record_summary_type balance_record_summary;
    for( const auto& balance_item : balance_record_summary )
    {
        const string& account_name = balance_item.first;
        for( const auto& balance_record : balance_item.second )
        {
            const balance_id_type& balance_id = balance_record.id();
            switch( withdraw_condition_types( balance_record.condition.type ) )
            {
                case withdraw_signature_type:
                    if( balance_record.snapshot_info.valid() )
                        account_balances[ balance_id ] = "GENESIS-" + balance_record.snapshot_info->original_address;
                    else
                        account_balances[ balance_id ] = account_name;
                    break;
                case withdraw_vesting_type:
                    if( balance_record.snapshot_info.valid() )
                    {
                        account_balances[ balance_id ] = "SHAREDROP-" + balance_record.snapshot_info->original_address;
                        break;
                    }
                    // else fall through
                default:
                    account_balances[ balance_id ] = balance_record.condition.type_label() + "-" + string( balance_id );
                    break;
            }
        }
    }

    set<string> account_names;
    const vector<wallet_account_record> accounts = self->list_accounts();
    for( const wallet_account_record& account : accounts )
        account_names.insert( account.name );

    return scan_transaction_experimental( eval_state, block_num, timestamp, account_keys,
                                          account_balances, account_names, overwrite_existing );
} FC_CAPTURE_AND_RETHROW( (eval_state)(block_num)(timestamp)(overwrite_existing) ) }

transaction_ledger_entry detail::wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                                             uint32_t block_num,
                                                                             const time_point_sec timestamp,
                                                                             const map<private_key_type, string>& account_keys,
                                                                             const map<address, string>& account_balances,
                                                                             const set<string>& account_names,
                                                                             bool overwrite_existing )
{ try {
    transaction_ledger_entry record;

    const transaction_id_type transaction_id = eval_state.trx.id();
    const transaction_id_type& record_id = transaction_id;
    const bool existing_record = _wallet_db.experimental_transactions.count( record_id ) > 0; // TODO
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

/* This is the master transaction scanning function. The brief layout:
 * - Perform some initialization
 * - Define lambdas for scanning each supported operation
 *   - These return true if the operation is relevant to this wallet
 * - Scan the transaction's operations
 * - Perform some cleanup and record-keeping
 */
void detail::wallet_impl::scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                         const map<private_key_type, string>& account_keys,
                                                         const map<address, string>& account_balances,
                                                         const set<string>& account_names,
                                                         transaction_ledger_entry& record,
                                                         bool store_record )
{ try {
    map<string, map<asset_id_type, share_type>> raw_delta_amounts;
    uint16_t op_index = 0;
    uint16_t withdrawal_count = 0;
    vector<memo_status> titan_memos;

    // Used by scan_withdraw and scan_deposit below
    const auto collect_balance = [&]( const balance_id_type& balance_id, const asset& delta_amount ) -> bool
    {
        if( account_balances.count( balance_id ) > 0 ) // First check canonical labels
        {
            // TODO: Need to save balance labels locally before emptying them so they can be deleted from the chain
            // OR keep the owner information around in the eval state and keep track of that somehow
            const string& delta_label = account_balances.at( balance_id );
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return true;
        }
        else if( record.delta_labels.count( op_index ) > 0 ) // Next check custom labels
        {
            const string& delta_label = record.delta_labels.at( op_index );
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return account_names.count( delta_label ) > 0;
        }
        else // Fallback to using the balance id as the label
        {
            // TODO: We should use the owner, not the ID -- also see case 1 above
            const string delta_label = string( balance_id );
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        }
    };

    const auto scan_withdraw = [&]( const withdraw_operation& op ) -> bool
    {
        ++withdrawal_count;
        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            return collect_balance( op.balance_id, delta_amount );
        };
        return eval_state.scan_op_deltas( op_index, scan_delta );
    };

    // TODO: Recipient address label and memo message needs to be saved at time of creation by sender
    // to do this make a wrapper around trx.deposit_to_account just like my->withdraw_to_transaction
    const auto scan_deposit = [&]( const deposit_operation& op ) -> bool
    {
        const balance_id_type& balance_id = op.balance_id();

        const auto scan_withdraw_with_signature = [&]( const withdraw_with_signature& condition ) -> bool
        {
            if( condition.memo.valid() )
            {
                bool stealth = false;
                omemo_status status;
                private_key_type recipient_key;

                try
                {
                    const private_key_type key = self->get_private_key( condition.owner );
                    status = condition.decrypt_memo_data( key, true );
                    if( status.valid() ) recipient_key = key;
                }
                catch( const fc::exception& )
                {
                }

                if( !status.valid() )
                {
                    vector<fc::future<void>> decrypt_memo_futures;
                    decrypt_memo_futures.resize( _stealth_private_keys.size() );

                    for( uint32_t key_index = 0; key_index < _stealth_private_keys.size(); ++key_index )
                    {
                        decrypt_memo_futures[ key_index ] = fc::async( [ &, key_index ]()
                        {
                            _scanner_threads[ key_index % _num_scanner_threads ]->async( [ & ]()
                            {
                                if( status.valid() ) return;
                                const private_key_type& key = _stealth_private_keys.at( key_index );
                                const omemo_status inner_status = condition.decrypt_memo_data( key );
                                if( inner_status.valid() )
                                {
                                    stealth = true;
                                    status = inner_status;
                                    recipient_key = key;
                                }
                            }, "decrypt memo" ).wait();
                        } );
                    }

                    for( auto& decrypt_memo_future : decrypt_memo_futures )
                    {
                        try
                        {
                            decrypt_memo_future.wait();
                        }
                        catch( const fc::exception& )
                        {
                        }
                    }
                }

                // If I've successfully decrypted then it's for me
                if( status.valid() )
                {
                    _wallet_db.cache_memo( *status, recipient_key, _wallet_password );

                    if( stealth ) titan_memos.push_back( *status );

                    const string& delta_label = account_keys.at( recipient_key );
                    record.delta_labels[ op_index ] = delta_label;

                    const string& memo = status->get_message();
                    if( !memo.empty() ) record.operation_notes[ op_index ] = memo;
                }
            }

            const auto scan_delta = [&]( const asset& delta_amount ) -> bool
            {
                return collect_balance( balance_id, delta_amount );
            };

            return eval_state.scan_op_deltas( op_index, scan_delta );
        };

        switch( withdraw_condition_types( op.condition.type ) )
        {
            case withdraw_signature_type:
                return scan_withdraw_with_signature( op.condition.as<withdraw_with_signature>() );
            // TODO: Other withdraw types
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
        const string& account_name = account_record.valid() ? account_record->name : std::to_string( op.account_id );

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
        const string& account_name = account_record.valid() ? account_record->name : std::to_string( op.account_id );

        const string delta_label = "INCOME-" + account_name;
        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return account_names.count( account_name ) > 0;
        };

        return eval_state.scan_op_deltas( op_index, scan_delta );
    };

    const auto scan_create_asset = [&]( const create_asset_operation& op ) -> bool
    {
        const oaccount_record account_record = _blockchain->get_account_record( op.issuer_account_id );
        const string& account_name = account_record.valid() ? account_record->name : string( "MARKET" );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = account_name + " created asset " + op.symbol;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_issue_asset = [&]( const issue_asset_operation& op ) -> bool
    {
        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            const oasset_record asset_record = _blockchain->get_asset_record( delta_amount.asset_id );
            string account_name = "GOD";
            if( asset_record.valid() )
            {
                const oaccount_record account_record = _blockchain->get_account_record( asset_record->issuer_id );
                account_name = account_record.valid() ? account_record->name : std::to_string( asset_record->issuer_id );
            }

            const string delta_label = "ISSUER-" + account_name;
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

            return account_names.count( account_name ) > 0;
        };

        return eval_state.scan_op_deltas( op_index, scan_delta );
    };

    const auto scan_bid = [&]( const bid_operation& op ) -> bool
    {
        const market_order order( bid_order, op.bid_index, op.amount );
        const string delta_label = "BID-" + string( order.get_id() );

        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        };
        eval_state.scan_op_deltas( op_index, scan_delta );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            string note;
            if( op.amount >= 0 )
            {
                const asset_id_type base_asset_id = op.bid_index.order_price.base_asset_id;
                const oasset_record& asset_record = _blockchain->get_asset_record( base_asset_id );
                const string& asset_symbol = asset_record.valid() ? asset_record->symbol : std::to_string( base_asset_id );
                note = "buy " + asset_symbol + " @ " + _blockchain->to_pretty_price( op.bid_index.order_price );
            }
            else
            {
                note = "cancel " + delta_label;
            }

            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record& key_record = _wallet_db.lookup_key( op.bid_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_ask = [&]( const ask_operation& op ) -> bool
    {
        const market_order order( ask_order, op.ask_index, op.amount );
        const string delta_label = "ASK-" + string( order.get_id() );

        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        };
        eval_state.scan_op_deltas( op_index, scan_delta );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            string note;
            if( op.amount >= 0 )
            {
                const asset_id_type base_asset_id = op.ask_index.order_price.base_asset_id;
                const oasset_record& asset_record = _blockchain->get_asset_record( base_asset_id );
                const string& asset_symbol = asset_record.valid() ? asset_record->symbol : std::to_string( base_asset_id );
                note = "sell " + asset_symbol + " @ " + _blockchain->to_pretty_price( op.ask_index.order_price );
            }
            else
            {
                note = "cancel " + delta_label;
            }

            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record& key_record = _wallet_db.lookup_key( op.ask_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_short = [&]( const short_operation& op ) -> bool
    {
        const market_order order( short_order, op.short_index, op.amount );
        const string delta_label = "SHORT-" + string( order.get_id() );

        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        };
        eval_state.scan_op_deltas( op_index, scan_delta );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            string note;
            if( op.amount >= 0 )
            {
                const price& interest_rate = op.short_index.order_price;
                const asset_id_type quote_asset_id = interest_rate.quote_asset_id;
                const oasset_record& asset_record = _blockchain->get_asset_record( quote_asset_id );
                const string& asset_symbol = asset_record.valid() ? asset_record->symbol : std::to_string( quote_asset_id );
                note = "short " + asset_symbol + " @ " + std::to_string( 100 * atof( interest_rate.ratio_string().c_str() ) ) + " %";
            }
            else
            {
                note = "cancel " + delta_label;
            }

            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record& key_record = _wallet_db.lookup_key( op.short_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    // TODO: Delete deprecated market_order::get_small_id()
    const auto scan_cover = [&]( const cover_operation& op ) -> bool
    {
        const market_order order( cover_order, op.cover_index, op.amount );
        const string delta_label = "MARGIN-" + string( order.get_id() );

        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        };
        eval_state.scan_op_deltas( op_index, scan_delta );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "repay " + delta_label;
            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record& key_record = _wallet_db.lookup_key( op.cover_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_add_collateral = [&]( const add_collateral_operation& op ) -> bool
    {
        const market_order order( cover_order, op.cover_index, op.amount );
        const string delta_label = "MARGIN-" + string( order.get_id() );

        const auto scan_delta = [&]( const asset& delta_amount ) -> bool
        {
            raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;
            return false;
        };
        eval_state.scan_op_deltas( op_index, scan_delta );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "collateralize " + delta_label;
            record.operation_notes[ op_index ] = note;
        }

        const owallet_key_record& key_record = _wallet_db.lookup_key( op.cover_index.owner );
        return key_record.valid() && key_record->has_private_key();
    };

    const auto scan_update_feed = [&]( const update_feed_operation& op ) -> bool
    {
        const oasset_record asset_record = _blockchain->get_asset_record( op.index.quote_id );
        const string& asset_symbol = asset_record.valid() ? asset_record->symbol : std::to_string( op.index.quote_id );

        const oaccount_record account_record = _blockchain->get_account_record( op.index.delegate_id );
        const string& account_name = account_record.valid() ? account_record->name : std::to_string( op.index.delegate_id );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "update " + account_name + "'s feed for " + asset_symbol;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    const auto scan_burn = [&]( const burn_operation& op ) -> bool
    {
        const string delta_label = "INCINERATOR";
        const asset& delta_amount = op.amount;
        raw_delta_amounts[ delta_label ][ delta_amount.asset_id ] += delta_amount.amount;

        const account_id_type account_id = op.account_id;
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

    const auto scan_update_signing_key = [&]( const update_signing_key_operation& op ) -> bool
    {
        const oaccount_record account_record = _blockchain->get_account_record( op.account_id );
        const string& account_name = account_record.valid() ? account_record->name : std::to_string( op.account_id );

        if( record.operation_notes.count( op_index ) == 0 )
        {
            const string note = "update signing key for " + account_name;
            record.operation_notes[ op_index ] = note;
        }

        return account_names.count( account_name ) > 0;
    };

    bool relevant_to_me = false;
    for( const auto& op : eval_state.trx.operations )
    {
        bool result = false;
        switch( operation_type_enum( op.type ) )
        {
            case withdraw_op_type:
                result = scan_withdraw( op.as<withdraw_operation>() );
                break;
            case deposit_op_type:
                result = scan_deposit( op.as<deposit_operation>() );
                break;
            case register_account_op_type:
                result = scan_register_account( op.as<register_account_operation>() );
                _dirty_accounts |= result;
                break;
            case update_account_op_type:
                result = scan_update_account( op.as<update_account_operation>() );
                _dirty_accounts |= result;
                break;
            case withdraw_pay_op_type:
                result = scan_withdraw_pay( op.as<withdraw_pay_operation>() );
                break;
            case create_asset_op_type:
                result = scan_create_asset( op.as<create_asset_operation>() );
                break;
            case issue_asset_op_type:
                result = scan_issue_asset( op.as<issue_asset_operation>() );
                break;
            case bid_op_type:
                result = scan_bid( op.as<bid_operation>() );
                break;
            case ask_op_type:
                result = scan_ask( op.as<ask_operation>() );
                break;
            case short_op_type:
                result = scan_short( op.as<short_operation>() );
                break;
            case cover_op_type:
                result = scan_cover( op.as<cover_operation>() );
                break;
            case add_collateral_op_type:
                result = scan_add_collateral( op.as<add_collateral_operation>() );
                break;
            case define_slate_op_type:
                // Don't care; do nothing
                break;
            case update_feed_op_type:
                result = scan_update_feed( op.as<update_feed_operation>() );
                break;
            case burn_op_type:
                result = scan_burn( op.as<burn_operation>() );
                break;
            case release_escrow_op_type:
                // TODO
                break;
            case update_signing_key_op_type:
                result = scan_update_signing_key( op.as<update_signing_key_operation>() );
                break;
            case update_balance_vote_op_type:
                // TODO
                break;
            default:
                // Ignore everything else
                break;
        }

        relevant_to_me |= result;
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

        for( uint16_t i = 0; i < eval_state.trx.operations.size(); ++i )
        {
            if( operation_type_enum( eval_state.trx.operations.at( i ).type ) == withdraw_op_type )
                record.delta_labels[ i ] = account_name;
        }

        return true;
    };

    if( rescan_with_titan_info() )
        return scan_transaction_experimental( eval_state, account_keys, account_balances, account_names, record, store_record );

    for( const auto& delta_item : eval_state.fees_paid )
    {
        const asset delta_amount( delta_item.second, delta_item.first );
        if( delta_amount.amount != 0 )
            raw_delta_amounts[ "FEE" ][ delta_amount.asset_id ] += delta_amount.amount;
    }

    for( const auto& item : raw_delta_amounts )
    {
        const string& delta_label = item.first;
        for( const auto& delta_item : item.second )
            record.delta_amounts[ delta_label ].emplace_back( delta_item.second, delta_item.first );
    }

    if( relevant_to_me && store_record ) // TODO
    {
        ulog( "wallet_transaction_record_v2:\n${rec}", ("rec",fc::json::to_pretty_string( record )) );
        _wallet_db.experimental_transactions[ record.id ] = record;
    }

    _dirty_balances |= relevant_to_me;
} FC_CAPTURE_AND_RETHROW( (eval_state)(account_balances)(account_names)(record)(store_record) ) }

transaction_ledger_entry detail::wallet_impl::apply_transaction_experimental( const signed_transaction& transaction )
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
    if( my->_wallet_db.experimental_transactions.count( record_id ) == 0 ) // TODO
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

    for( const auto& item : my->_wallet_db.experimental_transactions ) // TODO
    {
        history.insert( to_pretty_transaction_experimental( item.second ) );
    }

    const auto account_records = list_accounts();
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

            for( const asset& delta_amount : item.second )
                balances[ label ][ delta_amount.asset_id ] += delta_amount.amount;
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
        const string& delta_label = item.first;
        for( const asset& delta_amount : item.second )
        {
            if( delta_amount.amount >= 0)
                result.outputs.emplace_back( delta_label, delta_amount );
            else
                result.inputs.emplace_back( delta_label, -delta_amount );
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

} } // bts::wallet
