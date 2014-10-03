#pragma once

#include <stdint.h>

/* Comment out this line for a non-test network */
#define BTS_TEST_NETWORK

#define BTS_TEST_NETWORK_VERSION                            33

/** @file bts/blockchain/config.hpp
 *  @brief Defines global constants that determine blockchain behavior
 */
#define BTS_BLOCKCHAIN_VERSION                              109
#define BTS_BLOCKCHAIN_DATABASE_VERSION                     146

/**
 *  The address prepended to string representation of
 *  addresses.
 *
 *  Changing these parameters will result in a hard fork.
 */
#define BTS_ADDRESS_PREFIX                                  "XTS"
#define BTS_BLOCKCHAIN_SYMBOL                               "XTS"
#define BTS_BLOCKCHAIN_NAME                                 "BitShares XTS"
#define BTS_BLOCKCHAIN_DESCRIPTION                          "BitShares X Test Network"
#define BTS_BLOCKCHAIN_PRECISION                            100000
#define BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC       (60*60*24*2)
#define BTS_BLOCKCHAIN_MIN_YIELD_PERIOD_SEC                 (60*60*24) // 24 hours

#define BTS_BLOCKCHAIN_MIN_BURN_FEE                         BTS_BLOCKCHAIN_PRECISION * 1 // 1 XTS
#define BTS_BLOCKCHAIN_DEFAULT_RELAY_FEE                    10000 // XTS
#define BTS_BLOCKCHAIN_MINIMUM_SHORT_ORDER_SIZE             (BTS_BLOCKCHAIN_PRECISION*100)
//#define BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC                 (30*24*60*60) // 30 days * 24 hours * 60 minutes * 60 seconds = 1 month
#define BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC                 (2*60*60) // 2 hours for test network

/**
 * The number of delegates that the blockchain is designed to support
 */
#define BTS_BLOCKCHAIN_NUM_DELEGATES                        uint32_t(101)
#define BTS_BLOCKCHAIN_MAX_SLATE_SIZE                       BTS_BLOCKCHAIN_NUM_DELEGATES
#define BTS_BLOCKCHAIN_MIN_FEEDS                            ((BTS_BLOCKCHAIN_NUM_DELEGATES/2) + 1)
#define BTS_BLOCKCHAIN_MAX_UNDO_HISTORY                     (BTS_BLOCKCHAIN_NUM_DELEGATES*4)

#define BTS_BLOCKCHAIN_ENABLE_NEGATIVE_VOTES                false

/**
 * To prevent a delegate from producing blocks on split network,
 * we check the connection count.  This means no blocks get produced
 * until at least a minimum number of clients are on line.
 */
#define BTS_MIN_DELEGATE_CONNECTION_COUNT                   1

/**
 * Defines the number of seconds that should elapse between blocks
 */
#define BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC                   int64_t(10)

/**
 *  The maximum size of the raw data contained in the blockchain, this size is
 *  notional based upon the serilized size of all user-generated transactions in
 *  all blocks for one year.
 *
 *  Actual size on disk will likely be 2 or 3x this size due to indexes and other
 *  storeage overhead.
 *
 *  Adjusting this value will change the effective fee charged on transactions
 */
#define BTS_BLOCKCHAIN_MAX_SIZE                             (1024*1024*1024*int64_t(100)) // 100 GB
#define BTS_BLOCKCHAIN_MIN_NAME_SIZE                        1
#define BTS_BLOCKCHAIN_MAX_NAME_SIZE                        63
#define BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE                   (1024*64)
#define BTS_BLOCKCHAIN_MAX_MEMO_SIZE                        19 // bytes
#define BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE                      5 // characters
#define BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE                      3 // characters
#define BTS_BLOCKCHAIN_PROPOSAL_VOTE_MESSAGE_MAX_SIZE       1024 // bytes

/**
 *  The maximum amount that can be issued for user assets.
 *
 *  10^18 / 2^63 < 1  however, to support representing all share values as a double in
 *  languages like java script, we must stay within the epsilon so
 *
 *  10^15 / 2^53 < 1 allows all values to be represented as a double or an int64
 */
#define BTS_BLOCKCHAIN_MAX_SHARES                           (1000*1000*int64_t(1000)*1000*int64_t(1000))

/**
 * Initial shares read from the genesis block are scaled to this number. It is divided
 * by 100 so that new shares may be issued without exceeding BTS_BLOCKCHAIN_MAX_SHARES
 */
#define BTS_BLOCKCHAIN_INITIAL_SHARES                       (BTS_BLOCKCHAIN_MAX_SHARES/5)

/**
 *  The number of blocks expected per hour based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_HOUR                      ((60*60)/BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

/**
 *  The number of blocks expected per day based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_DAY                       (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR*int64_t(24))

/**
 * The number of blocks expected per year based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_YEAR                      (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*int64_t(365))

#define BTS_BLOCKCHAIN_AVERAGE_TRX_SIZE                     512 // just a random assumption used to calibrate TRX per SEC
#define BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND                   1  // (10)
#define BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE               10 // (BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

/** defines the maximum block size allowed, 2 MB per hour */
#define BTS_BLOCKCHAIN_MAX_BLOCK_SIZE                       (10 * BTS_BLOCKCHAIN_AVERAGE_TRX_SIZE * BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE )

#define BTS_BLOCKCHAIN_INACTIVE_FEE_APR                     10  // 10% per year

/**
    This constant defines the number of blocks a delegate must produce before
    they are expected to break even on registration costs with their earned income.

 *   Currently set to 2 hours of active block production to break even.
 */
#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE            (BTS_BLOCKCHAIN_BLOCKS_PER_DAY/12)

/**
    If you are going to create an asset, you expect that it will be used in transactions.  We would
    like to limit the issuance of assets that have no demand for transactions, so we charge a fee
    proportional to average transaction fees included in a block (AKA delegate pay).  This constant
    defines the number of blocks worth of transactions that justify the creation of a new asset.

    The fee is set such that the transactions of this asset alone could justify filling up 2 full weeks of
    block production.
 */
#define BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE               (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14)
