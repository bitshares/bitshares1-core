#include <bts/net/core_messages.hpp>

namespace bts { namespace net {

  const core_message_type_enum item_ids_inventory_message::type            = core_message_type_enum::item_ids_inventory_message_type;
  const core_message_type_enum blockchain_item_ids_inventory_message::type = core_message_type_enum::blockchain_item_ids_inventory_message_type;
  const core_message_type_enum fetch_blockchain_item_ids_message::type     = core_message_type_enum::fetch_blockchain_item_ids_message_type;
  const core_message_type_enum fetch_items_message::type                   = core_message_type_enum::fetch_items_message_type;
  const core_message_type_enum item_not_available_message::type            = core_message_type_enum::item_not_available_message_type;
  const core_message_type_enum hello_message::type                         = core_message_type_enum::hello_message_type;
  const core_message_type_enum connection_accepted_message::type           = core_message_type_enum::connection_accepted_message_type;
  const core_message_type_enum connection_rejected_message::type           = core_message_type_enum::connection_rejected_message_type;
  const core_message_type_enum address_request_message::type               = core_message_type_enum::address_request_message_type;
  const core_message_type_enum address_message::type                       = core_message_type_enum::address_message_type;
  const core_message_type_enum closing_connection_message::type            = core_message_type_enum::closing_connection_message_type;
  const core_message_type_enum current_time_request_message::type          = core_message_type_enum::current_time_request_message_type;
  const core_message_type_enum current_time_reply_message::type            = core_message_type_enum::current_time_reply_message_type;
  const core_message_type_enum check_firewall_message::type                = core_message_type_enum::check_firewall_message_type;
  const core_message_type_enum check_firewall_reply_message::type          = core_message_type_enum::check_firewall_reply_message_type;

} } // bts::client
