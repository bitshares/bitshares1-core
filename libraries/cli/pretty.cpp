#include <bts/cli/pretty.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace bts { namespace cli {

bool FILTER_OUTPUT_FOR_TESTS = false;

string pretty_line( int size )
{
    std::stringstream ss;
    ss << '\n';
    for( int i = 0; i < size; ++i ) ss << '-';
    ss << '\n';
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
    auto ptime = boost::posix_time::from_time_t( time_t ( timestamp.sec_since_epoch() ) );
    return boost::posix_time::to_iso_extended_string( ptime );
}

string pretty_percent( double part, double whole, int precision )
{
    const auto percent = 100 * part / whole;
    std::stringstream ss;
    ss << std::setprecision( precision ) << std::fixed << percent << " %";
    return ss.str();
}

string pretty_delegate_list( const vector<account_record>& delegate_records, cptr client )
{
    if( delegate_records.empty() ) return "No delegates found.\n";
    FC_ASSERT( client != nullptr );

    std::stringstream out;
    out << std::left;

    out << std::setw(  6 ) << "ID";
    out << std::setw( 32 ) << "NAME";
    out << std::setw( 15 ) << "APPROVAL";
    out << std::setw(  9 ) << "PRODUCED";
    out << std::setw(  9 ) << "MISSED";
    out << std::setw( 14 ) << "RELIABILITY";
    out << std::setw(  9 ) << "PAY RATE";
    out << std::setw( 16 ) << "PAY BALANCE";
    out << std::setw( 10 ) << "LAST BLOCK";

    out << pretty_line( 120 );

    const auto& asset_record = client->get_chain()->get_asset_record( asset_id_type() );
    FC_ASSERT( asset_record.valid() );
    const auto share_supply = asset_record->current_share_supply;

    for( const auto& delegate_record : delegate_records )
    {
        out << std::setw(  6 ) << delegate_record.id;
        out << std::setw( 32 ) << pretty_shorten( delegate_record.name, 31 );
        out << std::setw( 15 ) << pretty_percent( delegate_record.net_votes(), share_supply, 10 );

        const auto num_produced = delegate_record.delegate_info->blocks_produced;
        const auto num_missed = delegate_record.delegate_info->blocks_missed;
        out << std::setw(  9 ) << num_produced;
        out << std::setw(  9 ) << num_missed;
        out << std::setw( 14 ) << pretty_percent( num_produced, num_produced + num_missed, 2 );

        out << std::setw(  9 ) << pretty_percent( delegate_record.delegate_info->pay_rate, 100, 0 );
        const auto& pay_balance = asset( delegate_record.delegate_info->pay_balance );
        out << std::setw( 16 ) << client->get_chain()->to_pretty_asset( pay_balance );

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

    out << pretty_line( 115 );

    auto last_block_timestamp = block_records.front().timestamp;

    for( const auto& block_record : block_records )
    {
        if ( block_records.size() > 1 )
        {
          int missed_blocks = (block_record.timestamp - last_block_timestamp).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
          if( missed_blocks < 0 )
              missed_blocks *= -1;
          --missed_blocks;

          if( missed_blocks > 0 ) {
              out << "*** Missed " << missed_blocks << " block";
              if( missed_blocks > 1)
                  out << 's';
              out << " ***\n";
          }
          last_block_timestamp = block_record.timestamp;
        }

        out << std::setw(  8 ) << block_record.block_num;
        out << std::setw( 20 ) << pretty_timestamp( block_record.timestamp );

        const auto& delegate_name = client->blockchain_get_block_signee( block_record.block_num );
        out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );

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
            out << std::setw(  8 ) << block_record.latency;
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

    std::stringstream out;

    out << std::setw(  7 ) << std::right << "BLK" << ".";
    out << std::setw(  5 ) << std::left << "TRX";
    out << std::setw( 20 ) << "TIMESTAMP";
    out << std::setw( 20 ) << "FROM";
    out << std::setw( 20 ) << "TO";
    out << std::setw( 20 ) << "AMOUNT";
    out << std::setw( 20 ) << "FEE";
    out << std::setw( 39 ) << "MEMO";
    out << std::setw(  8 ) << "ID";

    out << pretty_line( 160 );

    for( const auto& transaction : transactions )
    {
        if( transaction.block_num > 0 )
        {
            out << std::setw( 7 ) << std::right << transaction.block_num << ".";
            out << std::setw( 5 ) << std::left << transaction.trx_num;
        }
        else
        {
            out << std::setw( 13 ) << "   PENDING";
        }

        out << std::setw( 20 ) << pretty_timestamp( fc::time_point_sec( transaction.received_time ) );
        out << std::setw( 20 ) << pretty_shorten( transaction.from_account, 19 );
        out << std::setw( 20 ) << pretty_shorten( transaction.to_account, 19 );
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( transaction.amount );
        out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( asset( transaction.fees ) );
        out << std::setw( 39 ) << pretty_shorten( transaction.memo_message, 38 );

        out << std::setw( 8 );
        if( FILTER_OUTPUT_FOR_TESTS ) out << "[redacted]";
        else out << string( transaction.trx_id ).substr( 0, 8 );

        out << "\n";
    }

    return out.str();
}

string pretty_vote_summary( const account_vote_summary_type& votes )
{
    if( votes.empty() ) return "No votes found.\n";

    std::stringstream out;
    out << std::left;

    out << std::setw( 32 ) << "DELEGATE";
    out << std::setw( 16 ) << "VOTES";

    out << pretty_line( 48 );

    for( const auto& vote : votes )
    {
        const auto& delegate_name = vote.first;
        const auto votes_for = vote.second;

        out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );
        out << std::setw( 16 ) << votes_for;

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
    out << "Registered: " << pretty_timestamp( record->registration_date ) << "\n";
    out << "Last Updated: " << fc::get_approximate_relative_time_string( record->last_update ) << "\n";
    out << "Owner Key: " << std::string( record->owner_key ) << "\n";

    /* Only print active key history if there are keys in the history which are not the owner key */
    if( record->active_key_history.size() > 1
        || record->active_key_history.begin()->second != record->owner_key )
    {
      out << "Active Key History:\n";

      for( const auto& key : record->active_key_history )
      {
          out << "  Key: " << std::string( key.second )
              << ", last used " << fc::get_approximate_relative_time_string( key.first ) << "\n";
      }
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

} } // bts::cli
