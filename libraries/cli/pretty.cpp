#include <bts/blockchain/time.hpp>
#include <bts/cli/pretty.hpp>

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace bts { namespace cli {

bool FILTER_OUTPUT_FOR_TESTS = false;

string pretty_line( int size, char c )
{
    std::stringstream ss;
    for( int i = 0; i < size; ++i ) ss << c;
    return ss.str();
}

string pretty_shorten( const string& str, size_t max_size )
{
    if( str.size() > max_size ) return str.substr( 0, max_size - 3 ) + "...";
    return str;
}

string pretty_timestamp( const time_point_sec& timestamp )
{
    if( FILTER_OUTPUT_FOR_TESTS ) return "[redacted]";
    return timestamp.to_iso_extended_string();
}

string pretty_age( const time_point_sec& timestamp, bool from_now, const string& suffix )
{
    if( FILTER_OUTPUT_FOR_TESTS )
    {
        return "[redacted]";
    }
    else if( from_now )
    {
        const auto now = blockchain::now();
        if( suffix.empty() )
            return fc::get_approximate_relative_time_string( timestamp, now );
        else
            return fc::get_approximate_relative_time_string( timestamp, now, " " + suffix );
    }

    return fc::get_approximate_relative_time_string( timestamp );
}

string pretty_percent( double part, double whole, int precision )
{
    FC_ASSERT( part >= 0 );
    FC_ASSERT( whole >= 0 );
    FC_ASSERT( precision >= 0 );
    FC_ASSERT( part <= whole );
    if( whole <= 0 ) return "N/A";
    const auto percent = 100 * part / whole;
    std::stringstream ss;
    ss << std::setprecision( precision ) << std::fixed << percent << " %";
    return ss.str();
}

string pretty_info( fc::mutable_variant_object info, cptr client )
{
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    if( !info["blockchain_head_block_timestamp"].is_null() )
    {
        const auto timestamp = info["blockchain_head_block_timestamp"].as<time_point_sec>();
        info["blockchain_head_block_timestamp"] = pretty_timestamp( timestamp );

        if( !info["blockchain_head_block_age"].is_null() )
            info["blockchain_head_block_age"] = pretty_age( timestamp, true, "old" );
    }

    const auto participation = info["blockchain_average_delegate_participation"].as<double>();
    info["blockchain_average_delegate_participation"] = pretty_percent( participation, 100 );

    const auto pay_rate = info["blockchain_delegate_pay_rate"].as<share_type>();
    info["blockchain_delegate_pay_rate"] = client->get_chain()->to_pretty_asset( asset( pay_rate ) );

    const auto fees = info["blockchain_accumulated_fees"].as<share_type>();
    info["blockchain_accumulated_fees"] = client->get_chain()->to_pretty_asset( asset( fees ) );

    if( !info["ntp_time"].is_null() )
    {
        const auto ntp_time = info["ntp_time"].as<time_point_sec>();
        info["ntp_time"] = pretty_timestamp( ntp_time );
    }

    if( !info["wallet_unlocked_until_timestamp"].is_null() )
    {
        const auto unlocked_until_timestamp = info["wallet_unlocked_until_timestamp"].as<time_point_sec>();
        info["wallet_unlocked_until_timestamp"] = pretty_timestamp( unlocked_until_timestamp );

        if( !info["wallet_unlocked_until"].is_null() )
            info["wallet_unlocked_until"] = pretty_age( unlocked_until_timestamp, true );
    }

    if( !info["wallet_next_block_production_timestamp"].is_null() )
    {
        const auto next_block_timestamp = info["wallet_next_block_production_timestamp"].as<time_point_sec>();
        info["wallet_next_block_production_timestamp"] = pretty_timestamp( next_block_timestamp );

        if( !info["wallet_next_block_production_time"].is_null() )
        {
            info["wallet_next_block_production_time"] = pretty_age( next_block_timestamp, true );
        }
    }

    out << fc::json::to_pretty_string( info ) << "\n";
    return out.str();
}

string pretty_blockchain_config( fc::mutable_variant_object config, cptr client )
{
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    const auto timestamp = config["genesis_timestamp"].as<time_point_sec>();
    config["genesis_timestamp"] = pretty_timestamp( timestamp );

    const auto min_fee = config["min_block_fee"].as<share_type>();
    config["min_block_fee"] = client->get_chain()->to_pretty_asset( asset( min_fee ) );

    const auto inactivity_fee = config["inactivity_fee_apr"].as<share_type>();
    config["inactivity_fee_apr"] = client->get_chain()->to_pretty_asset( asset( inactivity_fee ) );

    const auto priority_fee = config["priority_fee"].as<share_type>();
    config["priority_fee"] = client->get_chain()->to_pretty_asset( asset( priority_fee ) );

    const auto delegate_reg_fee = config["delegate_reg_fee"].as<share_type>();
    config["delegate_reg_fee"] = client->get_chain()->to_pretty_asset( asset( delegate_reg_fee ) );

    const auto asset_reg_fee = config["asset_reg_fee"].as<share_type>();
    config["asset_reg_fee"] = client->get_chain()->to_pretty_asset( asset( asset_reg_fee ) );

    const auto min_market_depth = config["min_market_depth"].as<share_type>();
    config["min_market_depth"] = client->get_chain()->to_pretty_asset( asset( min_market_depth ) );

    out << fc::json::to_pretty_string( config ) << "\n";
    return out.str();
}

string pretty_wallet_info( fc::mutable_variant_object info, cptr client )
{
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    if( !info["unlocked_until_timestamp"].is_null() )
    {
        const auto unlocked_until_timestamp = info["unlocked_until_timestamp"].as<time_point_sec>();
        info["unlocked_until_timestamp"] = pretty_timestamp( unlocked_until_timestamp );

        if( !info["unlocked_until"].is_null() )
            info["unlocked_until"] = pretty_age( unlocked_until_timestamp, true );
    }

    if( !info["scan_progress"].is_null() )
    {
        const auto scan_progress = info["scan_progress"].as<float>();
        info["scan_progress"] = pretty_percent( scan_progress, 1 );
    }

    if( !info["priority_fee"].is_null() )
    {
        const auto priority_fee = info["priority_fee"].as<asset>();
        info["priority_fee"] = client->get_chain()->to_pretty_asset( priority_fee );
    }

    if( !info["next_block_production_timestamp"].is_null() )
    {
        const auto next_block_timestamp = info["next_block_production_timestamp"].as<time_point_sec>();
        info["next_block_production_timestamp"] = pretty_timestamp( next_block_timestamp );

        if( !info["next_block_production_time"].is_null() )
        {
            info["next_block_production_time"] = pretty_age( next_block_timestamp, true );
        }
    }

    out << fc::json::to_pretty_string( info ) << "\n";
    return out.str();
}

string pretty_delegate_list( const vector<account_record>& delegate_records, cptr client )
{
    if( delegate_records.empty() ) return "No delegates found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw(  6 ) << "ID";
    out << std::setw( 32 ) << "NAME (* next in line)";
    out << std::setw( 15 ) << "APPROVAL";
    out << std::setw(  9 ) << "PRODUCED";
    out << std::setw(  9 ) << "MISSED";
    out << std::setw( 14 ) << "RELIABILITY";
    out << std::setw(  9 ) << "PAY RATE";
    out << std::setw( 20 ) << "PAY BALANCE";
    out << std::setw( 10 ) << "LAST BLOCK";
    out << "\n";

    out << pretty_line( 124 );
    out << "\n";

    const auto current_slot_timestamp = blockchain::get_slot_start_time( blockchain::now() );
    const auto head_block_timestamp = client->get_chain()->get_head_block().timestamp;
    const auto next_slot_time = std::max( current_slot_timestamp, head_block_timestamp + BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC );
    const auto& active_delegates = client->get_chain()->get_active_delegates();
    const auto next_slot_signee = client->get_chain()->get_slot_signee( next_slot_time, active_delegates );
    const auto next_signee_name = next_slot_signee.name;

    const auto asset_record = client->get_chain()->get_asset_record( asset_id_type() );
    FC_ASSERT( asset_record.valid() );
    const auto share_supply = asset_record->current_share_supply;

    for( const auto& delegate_record : delegate_records )
    {
        out << std::setw(  6 ) << delegate_record.id;

        const auto delegate_name = delegate_record.name;
        if( delegate_name != next_signee_name )
            out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );
        else
            out << std::setw( 32 ) << pretty_shorten( delegate_name, 29 ) + " *";

        out << std::setw( 15 ) << pretty_percent( delegate_record.net_votes(), share_supply, 10 );

        const auto num_produced = delegate_record.delegate_info->blocks_produced;
        const auto num_missed = delegate_record.delegate_info->blocks_missed;
        out << std::setw(  9 ) << num_produced;
        out << std::setw(  9 ) << num_missed;
        out << std::setw( 14 ) << pretty_percent( num_produced, num_produced + num_missed, 2 );

        out << std::setw(  9 ) << pretty_percent( delegate_record.delegate_info->pay_rate, 100, 0 );
        const auto pay_balance = asset( delegate_record.delegate_info->pay_balance );
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( pay_balance );

        const auto last_block = delegate_record.delegate_info->last_block_num_produced;
        out << std::setw( 10 ) << ( last_block > 0 ? std::to_string( last_block ) : "NONE" );

        out << "\n";
    }

    return out.str();
}

string pretty_block_list( const vector<block_record>& block_records, cptr client )
{
    if( block_records.empty() ) return "No blocks found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw(  8 ) << "HEIGHT";
    out << std::setw( 20 ) << "TIMESTAMP";
    out << std::setw( 32 ) << "SIGNING DELEGATE";
    out << std::setw(  8 ) << "# TXS";
    out << std::setw(  8 ) << "SIZE";
    out << std::setw( 16 ) << "TOTAL FEES";
    out << std::setw(  8 ) << "LATENCY";
    out << std::setw( 15 ) << "PROCESSING TIME";
    out << "\n";

    out << pretty_line( 115 );
    out << "\n";

    auto last_block_timestamp = block_records.front().timestamp;

    for( const auto& block_record : block_records )
    {
        /* Print any missed slots */

        const bool descending = last_block_timestamp > block_record.timestamp;
        while( last_block_timestamp != block_record.timestamp )
        {
            if( descending ) last_block_timestamp -= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
            else last_block_timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

            if( last_block_timestamp == block_record.timestamp ) break;

            out << std::setw(  8 ) << "MISSED";
            out << std::setw( 20 ) << pretty_timestamp( last_block_timestamp );

            const auto slot_record = client->get_chain()->get_slot_record( last_block_timestamp );
            FC_ASSERT( slot_record.valid() );
            const auto delegate_record = client->get_chain()->get_account_record( slot_record->block_producer_id );
            FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
            out << std::setw( 32 ) << pretty_shorten( delegate_record->name, 31 );

            out << std::setw(  8 ) << "N/A";
            out << std::setw(  8 ) << "N/A";
            out << std::setw( 16 ) << "N/A";
            out << std::setw(  8 ) << "N/A";
            out << std::setw( 15 ) << "N/A";
            out << '\n';
        }

        /* Print produced block */

        out << std::setw(  8 ) << block_record.block_num;
        out << std::setw( 20 ) << pretty_timestamp( block_record.timestamp );

        const auto& delegate_name = client->blockchain_get_block_signee( block_record.block_num );

        out << std::setw( 32 );
        if( FILTER_OUTPUT_FOR_TESTS ) out << "[redacted]";
        else out << pretty_shorten( delegate_name, 31 );

        out << std::setw(  8 ) << block_record.user_transaction_ids.size();
        out << std::setw(  8 ) << block_record.block_size;
        out << std::setw( 16 ) << client->get_chain()->to_pretty_asset( asset( block_record.total_fees ) );

        if( FILTER_OUTPUT_FOR_TESTS )
        {
            out << std::setw(  8 ) << "[redacted]";
            out << std::setw( 15 ) << "[redacted]";
        }
        else
        {
            out << std::setw(  8 ) << block_record.latency.to_seconds();
            out << std::setw( 15 ) << block_record.processing_time.count() / double( 1000000 );
        }

        out << '\n';
    }

    return out.str();
}

string pretty_transaction_list( const vector<pretty_transaction>& transactions, cptr client )
{
    if( transactions.empty() ) return "No transactions found.\n";
    FC_ASSERT( client != nullptr );

    const auto is_filtered = !transactions.front().ledger_entries.front().running_balances.empty();

    auto any_group = false;
    for( const auto& transaction : transactions )
        any_group |= transaction.ledger_entries.size() > 1;

    std::stringstream out;
    out << std::left;

    if( any_group ) out << " ";

    out << std::setw( 20 ) << "RECEIVED";
    out << std::setw( 10 ) << "BLOCK";
    out << std::setw( 20 ) << "FROM";
    out << std::setw( 20 ) << "TO";
    out << std::setw( 24 ) << "AMOUNT";
    out << std::setw( 44 ) << "MEMO";
    if( is_filtered ) out << std::setw( 24 ) << "BALANCE";
    out << std::setw( 20 ) << "FEE";
    out << std::setw(  8 ) << "ID";
    out << "\n";

    const auto line_size = !is_filtered ? 166 : 190;
    out << pretty_line( !any_group ? line_size : line_size + 2 ) << "\n";

    const auto errors = client->get_wallet()->get_pending_transaction_errors();

    auto group = true;
    for( const auto& transaction : transactions )
    {
        const auto prev_group = group;
        group = transaction.ledger_entries.size() > 1;
        if( group && !prev_group ) out << pretty_line( line_size + 2, '-' ) << "\n";

        auto count = 0;
        for( const auto& entry : transaction.ledger_entries )
        {
            const auto is_pending = !transaction.is_virtual && !transaction.is_confirmed;

            if( group ) out << "|";
            else if( any_group ) out << " ";

            ++count;
            if( count == 1 )
            {
                out << std::setw( 20 ) << pretty_timestamp( transaction.received_time );

                out << std::setw( 10 );
                if( !is_pending )
                {
                    out << transaction.block_num;
                }
                else if( errors.count( transaction.trx_id ) > 0 )
                {
                    auto name = string( errors.at( transaction.trx_id ).name() );
                    name = name.substr( 0, name.find( "_" ) );
                    boost::to_upper( name );
                    out << name.substr(0, 9 );
                }
                else
                {
                    out << "PENDING";
                }
            }
            else
            {
                out << std::setw( 20 ) << "";
                out << std::setw( 10 ) << "";
            }

            out << std::setw( 20 ) << pretty_shorten( entry.from_account, 19 );
            out << std::setw( 20 ) << pretty_shorten( entry.to_account, 19 );
            out << std::setw( 24 ) << client->get_chain()->to_pretty_asset( entry.amount );

            out << std::setw( 44 ) << pretty_shorten( entry.memo, 43 );

            if( is_filtered )
            {
                out << std::setw( 24 );
                if( !is_pending )
                    out << client->get_chain()->to_pretty_asset( entry.running_balances.at( entry.amount.asset_id ) );
                else
                    out << "N/A";
            }

            if( count == 1 )
            {
                out << std::setw( 20 );
                out << client->get_chain()->to_pretty_asset( transaction.fee );

                out << std::setw( 8 );
                if( FILTER_OUTPUT_FOR_TESTS )
                {
                    out << "[redacted]";
                }
                else if( transaction.is_virtual )
                {
                    std::stringstream ss;
                    ss << "[" << string( transaction.trx_id ).substr( 0, 6 ) << "]";
                    out << ss.str();
                }
                else
                {
                    out << string( transaction.trx_id ).substr( 0, 8 );
                }
            }
            else
            {
                out << std::setw( 20 ) << "";
                out << std::setw( 8 ) << "";
            }

            if( group ) out << "|";
            out << "\n";
        }

        if( group ) out << pretty_line( line_size + 2, '-' ) << "\n";
    }

    return out.str();
}

string pretty_asset_list( const vector<asset_record>& asset_records, cptr client )
{
    if( asset_records.empty() ) return "No assets found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw(  6 ) << "ID";
    out << std::setw(  7 ) << "SYMBOL";
    out << std::setw( 24 ) << "NAME";
    out << std::setw( 48 ) << "DESCRIPTION";
    out << std::setw( 32 ) << "ISSUER";
    out << std::setw( 10 ) << "ISSUED";
    out << std::setw( 28 ) << "SUPPLY";
    out << "\n";

    out << pretty_line( 155 );
    out << "\n";

    for( const auto& asset_record : asset_records )
    {
        const auto asset_id = asset_record.id;
        out << std::setw(  6 ) << asset_id;

        out << std::setw(  7 ) << asset_record.symbol;
        out << std::setw( 24 ) << pretty_shorten( asset_record.name, 23 );
        out << std::setw( 48 ) << pretty_shorten( asset_record.description, 47 );

        const auto issuer_id = asset_record.issuer_account_id;
        const auto supply = asset( asset_record.current_share_supply, asset_id );
        if( issuer_id == 0 )
        {
            out << std::setw( 32 ) << "GENESIS";
            out << std::setw( 10 ) << "N/A";
        }
        else if( issuer_id == asset_record::market_issued_asset )
        {
            out << std::setw( 32 ) << "MARKET";
            out << std::setw( 10 ) << "N/A";
        }
        else
        {
            const auto account_record = client->get_chain()->get_account_record( issuer_id );
            FC_ASSERT( account_record.valid() );
            out << std::setw( 32 ) << pretty_shorten( account_record->name, 31 );

            const auto max_supply = asset( asset_record.maximum_share_supply, asset_id );
            out << std::setw( 10 ) << pretty_percent( supply.amount, max_supply.amount);
        }

        out << std::setw( 28 ) << client->get_chain()->to_pretty_asset( supply );

        out << "\n";
    }

    return out.str();
}

string pretty_account( const oaccount_record& record, cptr client )
{
    if( !record.valid() ) return "No account found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << "Name: " << record->name << "\n";
    bool founder = record->registration_date == client->get_chain()->get_genesis_timestamp();
    string registered = !founder ? pretty_timestamp( record->registration_date ) : "Genesis (Keyhotee Founder)";
    out << "Registered: " << registered << "\n";
    out << "Last Updated: " << pretty_age( record->last_update ) << "\n";
    out << "Owner Key: " << std::string( record->owner_key ) << "\n";

    out << "Active Key History:\n";
    for( const auto& key : record->active_key_history )
    {
        out << "- " << std::string( key.second )
            << ", last used " << pretty_age( key.first ) << "\n";
    }

    if( record->is_delegate() )
    {
      const vector<account_record> delegate_records = { *record };
      out << "\n" << pretty_delegate_list( delegate_records, client ) << "\n";
    }
    else
    {
      out << "Not a delegate.\n";
    }

    if( !record->public_data.is_null() )
    {
      out << "Public Data:\n";
      out << fc::json::to_pretty_string( record->public_data ) << "\n";
    }

    return out.str();
}

// TODO: Print total at the end so that can be compared to (history)
string pretty_balances( const account_balance_summary_type& balances, cptr client )
{
    if( balances.empty() ) return "No balances found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw( 32 ) << "ACCOUNT";
    out << std::setw( 28 ) << "BALANCE";
    out << "\n";

    out << pretty_line( 60 );
    out << "\n";

    for( const auto& item : balances )
    {
        const auto& account_name = item.first;

        bool first = true;
        for( const auto& asset_balance : item.second.first )
        {
            if( first )
            {
                out << std::setw( 32 ) << pretty_shorten( account_name, 31 );
                first = false;
            }
            else
            {
                out << std::setw( 32 ) << "";
            }

            const auto balance = asset( asset_balance.second, client->get_chain()->get_asset_id( asset_balance.first ) );
            out << std::setw( 28 ) << client->get_chain()->to_pretty_asset( balance );

            out << "\n";
        }

        const auto& pay_balance = item.second.second;
        if( pay_balance > 0 )
        {
            out << "\b";
            out << std::setw( 32 ) << "";

            std::stringstream ss;
            ss << client->get_chain()->to_pretty_asset( asset( pay_balance ) ) << " (pay)";
            out << std::setw( 28 ) << ss.str() << "\n";
        }
    }

    return out.str();
}

string pretty_vote_summary( const account_vote_summary_type& votes, cptr client )
{
    if( votes.empty() ) return "No votes found.\n";

    std::stringstream out;
    out << std::left;

    out << std::setw( 32 ) << "DELEGATE";
    out << std::setw( 24 ) << "VOTES";
    out << std::setw(  8 ) << "APPROVED";
    out << "\n";

    out << pretty_line( 64 );
    out << "\n";

    for( const auto& vote : votes )
    {
        const auto& delegate_name = vote.first;
        const auto votes_for = vote.second;

        out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );
        out << std::setw( 24 ) << client->get_chain()->to_pretty_asset( asset( votes_for ) );
        out << std::setw(  8 ) << ( client->get_wallet()->get_delegate_approval( delegate_name ) ? "YES" : "NO" );

        out << "\n";
    }

    return out.str();
}

string pretty_market_orders( const vector<market_order>& market_orders, cptr client )
{ try {
    if( market_orders.empty() ) return "No market orders found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw( 12 ) << "TYPE";
    out << std::setw( 20 ) << "QUANTITY";
    out << std::setw( 30 ) << "PRICE";
    out << std::setw( 20 ) << "BALANCE";
    out << std::setw( 20 ) << "COST";
    out << std::setw( 20 ) << "COLLATERAL";
    out << std::setw( 36 ) << "ID";
    out << "\n";

    out << pretty_line( 128 );
    out << "\n";

    for( const auto& order : market_orders )
    {
        out << std::setw( 12 ) << variant( order.type ).as_string();
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( order.get_quantity() );
        out << std::setw( 30 ) << client->get_chain()->to_pretty_price( order.get_price() );
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( order.get_balance() );
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( order.get_quantity() * order.get_price() );
        if( order.type != cover_order )
           out << std::setw( 20 ) << "N/A";
        else
           out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( asset( *order.collateral ) );
        out << std::setw( 36 ) << fc::variant( order.market_index.owner ).as_string();

        out << "\n";
    }

    return out.str();
} FC_CAPTURE_AND_RETHROW(  ) }

} } // bts::cli
