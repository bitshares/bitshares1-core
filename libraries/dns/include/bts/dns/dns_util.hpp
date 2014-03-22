#pragma once

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/outputs.hpp>
#include <fc/reflect/variant.hpp>
#include <bts/dns/outputs.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_util.hpp>
#include <bts/blockchain/config.hpp>

#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>

//TODO obviously both temp
//need to be able to keep these low when in test mode
#define DNS_AUCTION_DURATION_BLOCKS (3)
#define DNS_EXPIRE_DURATION_BLOCKS  (8)

#define BTS_DNS_MIN_BID_FROM(bid)   ((11 * (bid)) / 10)
#define BTS_DNS_BID_FEE_RATIO(bid)  ((bid) / 2)

#define BTS_DNS_MAX_NAME_LEN        (128)
#define BTS_DNS_MAX_VALUE_LEN       (1024)

namespace bts { namespace dns {

bool name_exists(const std::string &name, const signed_transactions &outstanding_transactions, dns_db &db);

bool name_is_in_auction(const std::string &name, const signed_transactions &outstanding_transactions,
                        dns_db &db, trx_output &prev_output);

}} // bts::dns
