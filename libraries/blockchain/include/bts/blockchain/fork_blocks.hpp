#pragma once

#include <stdint.h>

/** @file bts/blockchain/fork_blocks.hpp
 *  @brief Defines global block number constants for when hardforks take effect
 */
#define BTSX_MARKET_FORK_1_BLOCK_NUM    274000
#define BTSX_MARKET_FORK_2_BLOCK_NUM    316001
#define BTSX_MARKET_FORK_3_BLOCK_NUM    340000
#define BTSX_MARKET_FORK_4_BLOCK_NUM    357000
#define BTSX_MARKET_FORK_5_BLOCK_NUM    408750
#define BTSX_MARKET_FORK_6_BLOCK_NUM    508750

#define BTSX_INTEREST_FORK_1_BLOCK_NUM    BTSX_MARKET_FORK_4_BLOCK_NUM
