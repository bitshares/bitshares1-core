#pragma once

/** @file bts/blockchain/config.hpp
 *  @brief Defines global constants that determine blockchain behavior
 */

#define BTS_BLOCKCHAIN_BLOCK_INTERVAL     (2ll)  // 2 minutes
#define BTS_BLOCKCHAIN_INITIAL_SHARES     (8000000*BTS_BLOCKCHAIN_SHARE)
#define BTS_BLOCKCHAIN_BLOCKS_PER_HOUR    (60/BTS_BLOCKCHAIN_BLOCK_INTERVAL)           
#define BTS_BLOCKCHAIN_BLOCKS_PER_DAY     (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR*24ll)
#define BTS_BLOCKCHAIN_BLOCKS_PER_YEAR    (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*365ll)
#define BTS_BLOCKCHAIN_SHARE              (100000000ll)
#define BTS_BLOCKCHAIN_DELEGATES          (10)

/** defines the maximum block size allowed. */
#define BTS_BLOCKCHAIN_MAX_BLOCK_SIZE     (1024*1024)

/** defines the target block size, fees will be adjusted to maintain this target */
#define BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE  (BTS_BLOCKCHAIN_MAX_BLOCK_SIZE/2)

/** set the minimum transation fee per byte to total .25% of the share supply */
#define BTS_BLOCKCHAIN_MIN_FEE            (BTS_BLOCKCHAIN_INITIAL_SHARES/400/512/1024)/BTS_BLOCKCHAIN_BLOCKS_PER_YEAR
