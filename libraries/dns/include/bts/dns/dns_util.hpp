#pragma once

#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/wallet/wallet.hpp>

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

bool is_dns_output(const trx_output &output);
claim_domain_output to_dns_output(const trx_output &output);

output_reference get_name_tx_ref(const std::string &name, dns_db &db);
trx_output get_tx_ref_output(const output_reference &tx_ref, dns_db &db);
uint32_t get_tx_age(const output_reference &tx_ref, dns_db &db);
uint32_t get_name_tx_age(const std::string &name, dns_db &db);

bool is_auction_age(uint32_t age);
bool is_expired_age(uint32_t age);
bool is_useable_age(uint32_t age);

std::vector<std::string> get_names_from_txs(const signed_transactions &txs);
std::vector<std::string> get_names_from_unspent(const std::map<bts::wallet::output_index, trx_output>
                                                &unspent_outputs);

bool name_is_available(const std::string &name, const std::vector<std::string> &name_pool, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref);
bool name_is_available(const std::string &name, const signed_transactions &tx_pool, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref);

bool name_is_useable(const std::string &name, const signed_transactions &tx_pool, dns_db &db,
                     const std::map<bts::wallet::output_index, trx_output> &unspent_outputs,
                     output_reference &prev_tx_ref);

std::vector<char> serialize_value(const fc::variant &value);

bool is_valid_amount(const asset &amount);
bool is_valid_name(const std::string &name);
bool is_valid_value(const std::vector<char> &value);
bool is_valid_value(const fc::variant &value);
bool is_valid_state(const fc::enum_type<uint8_t, claim_domain_output::states> state);

bool is_valid_bid_price(const asset &prev_bid_price, const asset &bid_price, asset &transfer_amount);

}} // bts::dns
