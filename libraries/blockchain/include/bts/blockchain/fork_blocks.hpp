/** @file bts/blockchain/fork_blocks.hpp
 *  @brief Defines global block number constants for when hardforks take effect
 */

#pragma once

#include <stdint.h>
#include <vector>

// TODO: Rename these constants based on public release version

#define BTS_EXPECTED_CHAIN_ID                   digest_type( "fe9ebbafee5edcb76430739325dc376ad4756f313da96e59359c3214891e8cfa" )
#define BTS_DESIRED_CHAIN_ID                    digest_type( "75c11a81b7670bbaa721cc603eadb2313756f94a3bcbb9928e9101432701ac5f" )

#define BTSX_MARKET_FORK_1_BLOCK_NUM            274000
#define BTSX_MARKET_FORK_2_BLOCK_NUM            316001
#define BTSX_MARKET_FORK_3_BLOCK_NUM            340000
#define BTSX_MARKET_FORK_4_BLOCK_NUM            357000
#define BTSX_MARKET_FORK_5_BLOCK_NUM            408750
#define BTSX_MARKET_FORK_6_BLOCK_NUM            451500
#define BTSX_MARKET_FORK_7_BLOCK_NUM            554800
#define BTSX_MARKET_FORK_8_BLOCK_NUM            578900
#define BTSX_MARKET_FORK_9_BLOCK_NUM            613200
#define BTSX_MARKET_FORK_10_BLOCK_NUM           640000
#define BTSX_MARKET_FORK_11_BLOCK_NUM           820200
#define BTSX_MARKET_FORK_12_BLOCK_NUM           871000
#define BTSX_MARKET_FORK_13_BLOCK_NUM           9999999

#define BTSX_MARKET_FORK_TO_UNIX_TIME_LIST ((BTSX_MARKET_FORK_1_BLOCK_NUM,  "0.4.0",      1408064036)) \
                                           ((BTSX_MARKET_FORK_2_BLOCK_NUM,  "0.4.9-RC1",  1409096675)) \
                                           ((BTSX_MARKET_FORK_3_BLOCK_NUM,  "0.4.9",      1409193626)) \
                                           ((BTSX_MARKET_FORK_4_BLOCK_NUM,  "0.4.10",     1409437355)) \
                                           ((BTSX_MARKET_FORK_5_BLOCK_NUM,  "0.4.12",     1409846462)) \
                                           ((BTSX_MARKET_FORK_6_BLOCK_NUM,  "0.4.13-RC1", 1410288486)) \
                                           ((BTSX_MARKET_FORK_7_BLOCK_NUM,  "0.4.16",     1411258737)) \
                                           ((BTSX_MARKET_FORK_8_BLOCK_NUM,  "0.4.17",     1411599233)) \
                                           ((BTSX_MARKET_FORK_9_BLOCK_NUM,  "0.4.18",     1411765631)) \
                                           ((BTSX_MARKET_FORK_10_BLOCK_NUM, "0.4.19",     1412203442)) \
                                           ((BTSX_MARKET_FORK_11_BLOCK_NUM, "0.4.21",     1413928884))

#define BTSX_YIELD_FORK_1_BLOCK_NUM             BTSX_MARKET_FORK_6_BLOCK_NUM
#define BTSX_YIELD_FORK_2_BLOCK_NUM             494000

#define BTSX_SUPPLY_FORK_1_BLOCK_NUM            BTSX_MARKET_FORK_7_BLOCK_NUM
#define BTSX_SUPPLY_FORK_2_BLOCK_NUM            BTSX_MARKET_FORK_8_BLOCK_NUM

namespace bts { namespace blockchain {
  uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t revision_time);
  std::vector<uint32_t> get_list_of_fork_block_numbers();
} }
