#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/time.hpp>

#include <string>

namespace bts { namespace cli {

using namespace bts::blockchain;
//using namespace bts::wallet;

std::string pretty_line( int size );
std::string pretty_shorten( const std::string& str, size_t max_size );
std::string pretty_timestamp( const time_point_sec& timestamp );
std::string pretty_seconds( const microseconds& ms );
std::string pretty_shares( const share_type& num_shares );
std::string pretty_percent( double part, double whole, int precision = 2 );

std::string pretty_delegate_list( const vector<account_record>& delegate_records, const share_type& share_supply );

std::string pretty_block_list( const vector<std::pair<block_record, std::string>>& items );

} } // bts::cli
