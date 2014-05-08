#pragma once

#include <bts/blockchain/config.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/time.hpp>

#include <vector>

namespace bts { namespace net {

  typedef fc::ripemd160 item_hash_t;
  struct item_id
  {
      uint32_t      item_type;
      item_hash_t   item_hash;

      item_id() {}
      item_id(uint32_t type, const item_hash_t& hash) :
        item_type(type),
        item_hash(hash)
      {}
      bool operator==(const item_id& other) const
      {
        return item_type == other.item_type &&
               item_hash == other.item_hash;
      }
  };

  enum core_message_type_enum
  {
    item_ids_inventory_message_type            = 5001,
    blockchain_item_ids_inventory_message_type = 5002,
    fetch_blockchain_item_ids_message_type     = 5003,
    fetch_item_message_type                    = 5004,
    item_not_available_message_type            = 5005,
    hello_message_type                         = 5006,
    hello_reply_message_type                   = 5007,
    connection_rejected_message_type           = 5008,
    address_request_message_type               = 5009,
    address_message_type                       = 5010,
  };

  const uint32_t core_protocol_version = BTS_NET_PROTOCOL_VERSION;

  struct item_ids_inventory_message
  {
    static const core_message_type_enum type;

    uint32_t item_type;
    std::vector<item_hash_t> item_hashes_available;

    item_ids_inventory_message() {}
    item_ids_inventory_message(uint32_t item_type, const std::vector<item_hash_t>& item_hashes_available) :
      item_type(item_type),
      item_hashes_available(item_hashes_available)
    {}
  };

  struct blockchain_item_ids_inventory_message
  {
    static const core_message_type_enum type;

    uint32_t total_remaining_item_count;
    uint32_t item_type;
    std::vector<item_hash_t> item_hashes_available;

    blockchain_item_ids_inventory_message() {}
    blockchain_item_ids_inventory_message(uint32_t total_remaining_item_count,
                                          uint32_t item_type,
                                          const std::vector<item_hash_t>& item_hashes_available) :
      total_remaining_item_count(total_remaining_item_count),
      item_type(item_type),
      item_hashes_available(item_hashes_available)
    {}
  };

  struct fetch_blockchain_item_ids_message
  {
    static const core_message_type_enum type;

    item_id last_item_seen;

    fetch_blockchain_item_ids_message() {}
    fetch_blockchain_item_ids_message(const item_id& last_item_seen) :
      last_item_seen(last_item_seen)
    {}
  };

  struct fetch_item_message
  {
    static const core_message_type_enum type;

    item_id item_to_fetch;

    fetch_item_message() {}
    fetch_item_message(const item_id& item_to_fetch) :
      item_to_fetch(item_to_fetch)
    {}
  };

  struct item_not_available_message
  {
    static const core_message_type_enum type;

    item_id requested_item;

    item_not_available_message() {}
    item_not_available_message(const item_id& requested_item) :
      requested_item(requested_item)
    {}
  };

  struct hello_message
  {
    static const core_message_type_enum type;

    std::string      user_agent;
    uint32_t         core_protocol_version;
    fc::ip::endpoint inbound_endpoint;
    fc::uint160_t    node_id;

    hello_message() {}
    hello_message(const std::string& user_agent, uint32_t core_protocol_version, fc::ip::endpoint inbound_endpoint, const fc::uint160_t& node_id) :
      user_agent(user_agent),
      core_protocol_version(core_protocol_version),
      inbound_endpoint(inbound_endpoint),
      node_id(node_id)
    {}
  };

  struct hello_reply_message
  {
    static const core_message_type_enum type;

    std::string      user_agent;
    uint32_t         core_protocol_version;
    fc::ip::endpoint remote_endpoint;
    fc::uint160_t node_id;

    hello_reply_message() {}
    hello_reply_message(const std::string& user_agent, uint32_t core_protocol_version,
                        fc::ip::endpoint remote_endpoint, const fc::uint160_t& node_id) :
      user_agent(user_agent),
      core_protocol_version(core_protocol_version),
      remote_endpoint(remote_endpoint),
      node_id(node_id)
    {}
  };

  struct connection_rejected_message
  {
    static const core_message_type_enum type;

    std::string      user_agent;
    uint32_t         core_protocol_version;
    fc::ip::endpoint remote_endpoint;

    connection_rejected_message() {}
    connection_rejected_message(const std::string& user_agent, uint32_t core_protocol_version,fc::ip::endpoint remote_endpoint) :
      user_agent(user_agent),
      core_protocol_version(core_protocol_version),
      remote_endpoint(remote_endpoint)
    {}
  };

  struct address_request_message
  {
    static const core_message_type_enum type;

    address_request_message() {}
  };

  struct address_info
  {
    fc::ip::endpoint   remote_endpoint;
    fc::time_point_sec last_seen_time;

    address_info() {}
    address_info(const fc::ip::endpoint& remote_endpoint, fc::time_point_sec last_seen_time) :
      remote_endpoint(remote_endpoint),
      last_seen_time(last_seen_time)
    {}
  };

  struct address_message
  {
    static const core_message_type_enum type;

    std::vector<address_info> addresses;
  };

} } // bts::client

FC_REFLECT_ENUM( bts::net::core_message_type_enum, (item_ids_inventory_message_type)(blockchain_item_ids_inventory_message_type)(fetch_blockchain_item_ids_message_type)(fetch_item_message_type)(hello_message_type)(address_request_message_type))
FC_REFLECT( bts::net::item_id, (item_type)(item_hash) )
FC_REFLECT( bts::net::item_ids_inventory_message, (item_type)(item_hashes_available) )
FC_REFLECT( bts::net::blockchain_item_ids_inventory_message, (total_remaining_item_count)(item_type)(item_hashes_available) )
FC_REFLECT( bts::net::fetch_blockchain_item_ids_message, (last_item_seen) )
FC_REFLECT( bts::net::fetch_item_message, (item_to_fetch) )
FC_REFLECT( bts::net::item_not_available_message, (requested_item) )
FC_REFLECT( bts::net::hello_message, (user_agent)(core_protocol_version)(inbound_endpoint)(node_id) )
FC_REFLECT( bts::net::hello_reply_message, (user_agent)(core_protocol_version)(remote_endpoint)(node_id) )
FC_REFLECT( bts::net::connection_rejected_message, (user_agent)(core_protocol_version)(remote_endpoint) )
FC_REFLECT_EMPTY( bts::net::address_request_message )
FC_REFLECT( bts::net::address_info, (remote_endpoint)(last_seen_time) )
FC_REFLECT( bts::net::address_message, (addresses) )

#include <unordered_map>
#include <fc/crypto/city.hpp>
namespace std
{
    template<>
    struct hash<bts::net::item_id>
    {
       size_t operator()(const bts::net::item_id& item_to_hash) const
       {
//#if defined(_M_X64) || defined(__x86_64__)
          return fc::city_hash64((char*)&item_to_hash, sizeof(item_to_hash));
//#else
//          return fc::city_hash32((char*)&item_to_hash, sizeof(item_to_hash));
//#endif
       }
    };
}
