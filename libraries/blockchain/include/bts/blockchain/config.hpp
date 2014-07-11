#pragma once

#include <stdint.h>

/** @file bts/blockchain/config.hpp
 *  @brief Defines global constants that determine blockchain behavior
 */
#define BTS_BLOCKCHAIN_VERSION                              (105)
#define BTS_WALLET_VERSION                                  (100)
#define BTS_BLOCKCHAIN_DATABASE_VERSION                     (114)

/**
 *  The address prepended to string representation of
 *  addresses.
 *
 *  Changing these parameters will result in a hard fork.
 */
#define BTS_ADDRESS_PREFIX                                 "BTSX"
#define BTS_BLOCKCHAIN_SYMBOL                              "BTSX"
#define BTS_BLOCKCHAIN_NAME                                "BitShares X"
#define BTS_BLOCKCHAIN_DESCRIPTION                         "Decentralized Autonomous Exchange"
#define BTS_BLOCKCHAIN_PRECISION                           (100000)
#define BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC      (60*60*24*2)
#define BTS_BLOCKCHAIN_DEFAULT_TRANSACTION_EXPIRATION_SEC  (60*60*2)

#define BTS_BLOCKCHAIN_DEFAULT_PRIORITY_FEE                 (10000) // XTS

/**
 * The number of delegates that the blockchain is designed to support
 */
#define BTS_BLOCKCHAIN_NUM_DELEGATES                        (101)
#define BTS_BLOCKCHAIN_MAX_SLATE_SIZE                       (BTS_BLOCKCHAIN_NUM_DELEGATES)


/**
 * To prevent a delegate from producing blocks on split network,
 * we check the connection count.  This means no blocks get produced
 * until at least a minimum number of clients are on line.
 */
#define BTS_MIN_DELEGATE_CONNECTION_COUNT               (1)

/**
 * Defines the number of seconds that should elapse between blocks
 */
#define BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC               int64_t(30)

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
#define BTS_BLOCKCHAIN_MAX_SIZE                             (1024*1024*1024*100ll) // 100 GB
#define BTS_BLOCKCHAIN_MIN_NAME_SIZE                        (1)
#define BTS_BLOCKCHAIN_MAX_NAME_SIZE                        (63)
#define BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE                   (1024*64)
#define BTS_BLOCKCHAIN_MAX_MEMO_SIZE                        (19) // bytes
#define BTS_BLOCKCHAIN_MAX_SYMBOL_SIZE                      (5) // characters
#define BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE                      (3) // characters
#define BTS_BLOCKCHAIN_PROPOSAL_VOTE_MESSAGE_MAX_SIZE       (1024) // bytes

/**
 *  The maximum amount that can be issued for user assets.
 *
 *  10^18 / 2^63 < 1
 */
#define BTS_BLOCKCHAIN_MAX_SHARES                           (1000*1000*1000ll*1000*1000ll)

/**
 * Initial shares read from the genesis block are scaled to this number. It is divided
 * by 100 so that new shares may be issued without exceeding BTS_BLOCKCHAIN_MAX_SHARES
 */
#define BTS_BLOCKCHAIN_INITIAL_SHARES                       (BTS_BLOCKCHAIN_MAX_SHARES / 5)

/**
 *  The number of blocks expected per hour based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_HOUR                      ((60*60)/BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

/**
 *  The number of blocks expected per day based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_DAY                       (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR*24ll)

/**
 * The number of blocks expected per year based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_YEAR                      (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*365ll)

#define BTS_BLOCKCHAIN_AVERAGE_TRX_SIZE                     (512) // just a random assumption used to calibrate TRX per SEC
#define BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND                   (10) 
#define BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE               (BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

/** defines the maximum block size allowed, 2 MB per hour */
#define BTS_BLOCKCHAIN_MAX_BLOCK_SIZE                       (BTS_BLOCKCHAIN_AVERAGE_TRX_SIZE * BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE )
#define BTS_BLOCKCHAIN_MAX_TRANSACTION_SIZE                 ( BTS_BLOCKCHAIN_MAX_BLOCK_SIZE / 2 )

/** defines the target block size, fees will be adjusted to maintain this target */
#define BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE                    (BTS_BLOCKCHAIN_MAX_BLOCK_SIZE/2)

#define BTS_BLOCKCHAIN_INACTIVE_FEE_APR                     (10)  // 10% per year

/**
 *  defines the min fee in milli-shares per byte
 */
#define BTS_BLOCKCHAIN_MIN_FEE                              (1000)

/**
    This constant defines the number of blocks a delegate must produce before
    they are expected to break even on registration costs with their earned income.

 *   Currently set to 2 weeks of active block production to break even.
 */
#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE            (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14)

/**
    If you are going to create an asset, you expect that it will be used in transactions.  We would
    like to limit the issuance of assets that have no demand for transactions, so we charge a fee
    proportional to average transaction fees included in a block (AKA delegate pay).  This constant
    defines the number of blocks worth of transactions that justify the creation of a new asset.

    The fee is set such that the transactions of this asset alone could justify filling up 2 full weeks of
    block production.
 */
#define BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE               (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14)
