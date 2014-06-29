#include <bts/cli/pretty.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace bts { namespace cli {

bool FILTER_OUTPUT_FOR_TESTS = false;

std::string pretty_line( int size )
{
    std::stringstream ss;
    ss << '\n';
    for( int i = 0; i < size; ++i ) ss << '-';
    ss << '\n';
    return ss.str();
}

std::string pretty_shorten( const std::string& str, size_t max_size )
{
    if( str.size() > max_size ) return str.substr(0, max_size - 3) + "...";
    return str;
}

std::string pretty_timestamp( const time_point_sec& timestamp )
{
    if( FILTER_OUTPUT_FOR_TESTS ) return "[redacted]";
    auto ptime = boost::posix_time::from_time_t( time_t ( timestamp.sec_since_epoch() ) );
    return boost::posix_time::to_iso_extended_string( ptime );
}

std::string pretty_seconds( const microseconds& ms )
{
    if( FILTER_OUTPUT_FOR_TESTS ) return "[redacted]";
    return std::to_string( ms.count() / double( 1000000 ) );
}

std::string pretty_shares( const share_type& num_shares )
{
    return std::to_string( num_shares / double( BTS_BLOCKCHAIN_PRECISION ) );
}

std::string pretty_percent( double part, double whole, int precision )
{
    const auto percent = 100 * part / whole;
    std::stringstream ss;
    ss << std::setprecision( precision ) << std::fixed << percent;
    return ss.str();
}

std::string pretty_delegate_list( const vector<account_record>& delegate_records, const share_type& share_supply )
{
    std::stringstream out;
    out << std::left;

    out << std::setw(6) << "ID";
    out << std::setw(32) << "NAME";
    out << std::setw(13) << "APPROVAL";
    out << std::setw(9) << "PRODUCED";
    out << std::setw(7) << "MISSED";
    out << std::setw(12) << "RELIABILITY";
    out << std::setw(9) << "PAY RATE";
    out << std::setw(12) << "PAY BALANCE";
    out << std::setw(10) << "LAST BLOCK";

    out << pretty_line( 110 );

    for( const auto& delegate_record : delegate_records )
    {
        out << std::setw(6)  << delegate_record.id;
        out << std::setw(32) << pretty_shorten( delegate_record.name, 32 );
        out << std::setw(13) << pretty_percent( delegate_record.net_votes(), share_supply, 10 );

        const auto num_produced = delegate_record.delegate_info->blocks_produced;
        const auto num_missed = delegate_record.delegate_info->blocks_missed;
        out << std::setw(9) << num_produced;
        out << std::setw(7) << num_missed;
        out << std::setw(12) << pretty_percent( num_produced, num_produced + num_missed );

        out << std::setw(9) << uint32_t( delegate_record.delegate_info->pay_rate );
        out << std::setw(12) << pretty_shares( delegate_record.delegate_info->pay_balance );

        const auto last_block = delegate_record.delegate_info->last_block_num_produced;
        out << std::setw(10) << ( (last_block > 0 && last_block != -1 ) ? std::to_string( last_block ) : "none" );

        out << "\n";
    }

    return out.str();
}

std::string pretty_block_list( const vector<std::pair<block_record, std::string>>& items )
{
    std::stringstream out;
    out << std::left;

    out << std::setw(8) << "HEIGHT";
    out << std::setw(20) << "TIMESTAMP";
    out << std::setw(32) << "SIGNING DELEGATE";
    out << std::setw(8) << "# TXS";
    out << std::setw(8)  << "SIZE";
    out << std::setw(16) << "TOTAL FEES";
    out << std::setw(8)  << "LATENCY";
    out << std::setw(15)  << "PROCESSING TIME";

    out << pretty_line( 115 );

    for( const auto& item : items )
    {
        const auto& block_record = item.first;
        const auto& delegate_name = item.second;

        out << std::setw(8) << block_record.block_num;
        out << std::setw(20) << pretty_timestamp( block_record.timestamp );
        out << std::setw(32) << pretty_shorten( delegate_name, 32 );
        out << std::setw(8) << block_record.user_transaction_ids.size();
        out << std::setw(8) << block_record.block_size;
        out << std::setw(16) << pretty_shares( block_record.total_fees );
        out << std::setw(8) << block_record.latency;
        out << std::setw(15) << pretty_seconds( block_record.processing_time );

        out << '\n';
    }

    return out.str();
}

} } // bts::cli
