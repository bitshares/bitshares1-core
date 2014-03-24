#pragma once

#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction.hpp>

#include <bts/dns/dns_db.hpp>
#include <bts/dns/outputs.hpp>

//TODO obviously both temp; need to be able to keep these low when in test mode
#define DNS_MAX_NAME_LEN            128
#define DNS_MAX_VALUE_LEN           1024

#define DNS_AUCTION_DURATION_BLOCKS 3
#define DNS_EXPIRE_DURATION_BLOCKS  8

#define DNS_ASSET_ZERO              (asset(uint64_t(0)))
#define DNS_MIN_BID_FROM(bid)       ((11 * (bid)) / 10)
#define DNS_BID_FEE_RATIO(bid)      ((bid) / 2)

namespace bts { namespace dns {

output_reference get_tx_ref(const std::string &name, dns_db &db);
uint32_t get_tx_age(const output_reference &tx_ref, dns_db &db);

trx_output fetch_output(const output_reference &tx_ref, dns_db &db);
bool is_claim_domain(const trx_output &output);
claim_domain_output output_to_dns(const trx_output &output);

bool is_auction_age(const uint32_t &age);
bool is_expired_age(const uint32_t &age);

bool name_is_in_txs(const std::string &name, const signed_transactions &txs);
bool can_bid_on_name(const std::string &name, const signed_transactions &txs, dns_db &db, bool &name_exists,
                     trx_output &prev_output, uint32_t &prev_output_age);

std::vector<char> serialize_value(const fc::variant &value);

// TODO: Move into validator
bool is_valid_amount(const asset &amount);
bool is_valid_name(const std::string &name);
bool is_valid_value(const std::vector<char> &value);
bool is_valid_value(const fc::variant &value);

bool is_valid_bid(const trx_output &output, const signed_transactions &txs, dns_db &db, bool &name_exists,
                  trx_output &prev_output, uint32_t &prev_output_age);

bool is_valid_ask(const trx_output &output, const signed_transactions &txs, dns_db &db, bool &name_exists,
                  trx_output &prev_output);

bool is_valid_set(const trx_output &output, const signed_transactions &txs, dns_db &db, bool &name_exists,
                  trx_output &prev_output);

}} // bts::dns
