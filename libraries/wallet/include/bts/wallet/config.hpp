#pragma once

#include <bts/wallet/config.hpp>

#define BTS_WALLET_VERSION                              uint32_t( 107 )

#define BTS_WALLET_MIN_PASSWORD_LENGTH                  8
#define BTS_WALLET_MIN_BRAINKEY_LENGTH                  32

#define BTS_WALLET_DEFAULT_UNLOCK_TIME_SEC              (60*60)

#define BTS_WALLET_DEFAULT_TRANSACTION_FEE              50000 // XTS

#define BTS_WALLET_DEFAULT_TRANSACTION_EXPIRATION_SEC   (BTS_BLOCKCHAIN_MAX_UNDO_HISTORY * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)

#define BTS_WALLET_NUM_SCANNING_THREADS                 4
