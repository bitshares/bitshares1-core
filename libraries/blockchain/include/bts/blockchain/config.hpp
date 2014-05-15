#pragma once

/** @file bts/blockchain/config.hpp
 *  @brief Defines global constants that determine blockchain behavior
 */
#define BTS_BLOCKCHAIN_VERSION                      (100)
#define BTS_WALLET_VERSION                          (100)

/**
 *  The address prepended to string representation of
 *  addresses.
 */
#define BTS_ADDRESS_PREFIX                          "XTS"

/**
 * Defines the number of seconds that should elapse between blocks
 */
#define BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC           (5ll)  // 30 seconds

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
#define BTS_BLOCKCHAIN_MAX_SIZE                     (1024*1024*1024*100ll) // 100 GB

/**
 * Initial shares read from the genesis block are scaled to this number.
 */
#define BTS_BLOCKCHAIN_INITIAL_SHARES               (80*1000*1000ll*1000*1000ll)

/**
 *  The maximum amount that can be issued for user assets.
 */
#define BTS_BLOCKCHAIN_MAX_SHARES                   (1000*1000*1000ll*1000*1000ll)

/**
 *  The number of blocks expected per hour based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_HOUR              ((60*60)/BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

/**
 *  The number of blocks expected per day based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_DAY               (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR*24ll)

/**
 * The number of blocks expected per year based upon the BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC
 */
#define BTS_BLOCKCHAIN_BLOCKS_PER_YEAR              (BTS_BLOCKCHAIN_BLOCKS_PER_DAY*365ll)

/**
 * The number of delegates that the blockchain is designed to support
 */
#define BTS_BLOCKCHAIN_NUM_DELEGATES                (10)

/**
 * A BIP is one 1/2^15 of the share supply at any given time.
 */
#define BTS_BLOCKCHAIN_BIP                          (1000*1000ll*1000*1000*1000ll)

/** defines the maximum block size allowed, 24 MB per hour */
#define BTS_BLOCKCHAIN_MAX_BLOCK_SIZE               (24 * 1024*1024 / BTS_BLOCKCHAIN_BLOCKS_PER_HOUR)

/** defines the target block size, fees will be adjusted to maintain this target */
#define BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE            (BTS_BLOCKCHAIN_MAX_BLOCK_SIZE/2)

/**
 *  defines the min fee in milli-shares per byte
 */
#define BTS_BLOCKCHAIN_MIN_FEE                      (1000)

/**
 *  the minimum mining reward paid to delegates, may result in some inflation
 *  if there is no transaction volume.  So long as there are atleast 2KB of
 *  transactions per block then there will be no inflation.
 */
#define BTS_BLOCKCHAIN_MIN_REWARD                   (200)

/**
 *  Defines the fee required to register a name to be considered for a delegate.  Having users vote
 *  for non-serious delegates is a waist of everyones time and money, this fee is set so that a
 *  delegate that is elected and produces blocks for 10 days can break even.  Any delegate that cannot
 *  perform reliably for 10 days should lose money.
 */
#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE    (BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE*BTS_BLOCKCHAIN_BLOCKS_PER_DAY / BTS_BLOCKCHAIN_NUM_DELEGATES )

/**
 *  Defines the fee required to register a asset, this fee is set to discourage anyone from registering all of the symbols and is equal to the fee of all
 *  blocks produced in a day. Only serious assets should register.  When transaction fees are low this fee should be low, as transaction fees rise the
 *  cost of issuing a new asset grows because the demand for transactions on this chain is already pretty high.
 */
#define BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE       (BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE*BTS_BLOCKCHAIN_BLOCKS_PER_DAY)


#define BTS_BLOCKCHAIN_MAX_NAME_SIZE                (63)
#define BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE           (1024*4)
