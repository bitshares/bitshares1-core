/** @file bts/blockchain/fork_blocks.hpp
 *  @brief Defines global block number constants for when hardforks take effect
 */

#pragma once

#include <stdint.h>
#include <vector>

#define BTS_EXPECTED_CHAIN_ID       digest_type( "6984e235f2573526db030e92f4844312ee93d63068c5224506a34a9c0b7b5db7" )
#define BTS_DESIRED_CHAIN_ID        digest_type( "2fbac26f05df0f2e8a66794d7d9eeae8a14d1478c040fb9ec02b4e6a9b09971e" )

#define BTS_V0_4_0_FORK_BLOCK_NUM        0
#define BTS_V0_4_9_FORK_BLOCK_NUM        0
#define BTS_V0_4_9_FORK_2_BLOCK_NUM      0
#define BTS_V0_4_10_FORK_BLOCK_NUM       0
#define BTS_V0_4_12_FORK_BLOCK_NUM       0
#define BTS_V0_4_13_FORK_BLOCK_NUM       0
#define BTS_V0_4_15_FORK_BLOCK_NUM       0
#define BTS_V0_4_16_FORK_BLOCK_NUM       0
#define BTS_V0_4_17_FORK_BLOCK_NUM       0
#define BTS_V0_4_18_FORK_BLOCK_NUM       0
#define BTS_V0_4_19_FORK_BLOCK_NUM       0
#define BTS_V0_4_21_FORK_BLOCK_NUM       0
#define BTS_V0_4_23_FORK_BLOCK_NUM       0
#define BTS_V0_4_24_FORK_BLOCK_NUM       0
#define BTS_V0_4_26_FORK_BLOCK_NUM       0
#define BTS_V0_5_0_FORK_BLOCK_NUM    25000
#define BTS_V0_6_0_FORK_BLOCK_NUM   120000

#define BTS_V0_4_28_FORK_BLOCK_NUM  9999999
#define BTS_V0_4_29_FORK_BLOCK_NUM  9999999

#define BTS_FORK_TO_UNIX_TIME_LIST  ((BTS_V0_4_0_FORK_BLOCK_NUM,   "0.4.0",     1408064036)) \
                                    ((BTS_V0_4_9_FORK_2_BLOCK_NUM, "0.4.9",     1409193626)) \
                                    ((BTS_V0_4_10_FORK_BLOCK_NUM,  "0.4.10",    1409437355)) \
                                    ((BTS_V0_4_12_FORK_BLOCK_NUM,  "0.4.12",    1409846462)) \
                                    ((BTS_V0_4_13_FORK_BLOCK_NUM,  "0.4.13",    1410294635)) \
                                    ((BTS_V0_4_15_FORK_BLOCK_NUM,  "0.4.15",    1410657316)) \
                                    ((BTS_V0_4_16_FORK_BLOCK_NUM,  "0.4.16",    1411258737)) \
                                    ((BTS_V0_4_17_FORK_BLOCK_NUM,  "0.4.17",    1411599233)) \
                                    ((BTS_V0_4_18_FORK_BLOCK_NUM,  "0.4.18",    1411765631)) \
                                    ((BTS_V0_4_19_FORK_BLOCK_NUM,  "0.4.19",    1412203442)) \
                                    ((BTS_V0_4_21_FORK_BLOCK_NUM,  "0.4.21",    1414019090))
                                    //((BTS_V0_4_23_FORK_BLOCK_NUM,  "0.4.23",    1414527136)) \
                                    //((BTS_V0_4_24_FORK_BLOCK_NUM,  "0.4.24",    1415731817)) \
                                    //((BTS_V0_4_26_FORK_BLOCK_NUM,  "0.4.26",    1418402446))

namespace bts { namespace blockchain {
  uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t revision_time);
  std::vector<uint32_t> get_list_of_fork_block_numbers();
} }
