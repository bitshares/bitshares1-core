#pragma once

/** @file bts/blockchain/config.hpp
 *  @brief Defines global constants that determine blockchain behavior
 */

#define BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC        (30ll)  // 30 seconds 
#define BTS_BLOCKCHAIN_MAX_SIZE                  (1024*1024*1024*100ll) // 100 GB
#define BTS_BLOCKCHAIN_INITIAL_SHARES            (8000000*10000000ll)
#define BTS_BLOCKCHAIN_BLOCKS_PER_HOUR           ((60*60)/BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)           
#define BTS_BLOCKCHAIN_BLOCKS_PER_DAY            (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR*24ll)
#define BTS_BLOCKCHAIN_BLOCKS_PER_YEAR           (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*365ll)
#define BTS_BLOCKCHAIN_DELEGATES                 (100)
#define BTS_BLOCKCHAIN_BIP                       (1000000000000000ll)

/** defines the maximum block size allowed, 24 MB per hour */
#define BTS_BLOCKCHAIN_MAX_BLOCK_SIZE     (24 * 1024*1024 / BTS_BLOCKCHAIN_BLOCKS_PER_HOUR)

/** defines the target block size, fees will be adjusted to maintain this target */
#define BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE  (BTS_BLOCKCHAIN_MAX_BLOCK_SIZE/2)

/**
 *  defines the min fee in shares
 */
#define BTS_BLOCKCHAIN_MIN_FEE                   1
#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE (BTS_BLOCKCHAIN_MIN_FEE*BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE)
