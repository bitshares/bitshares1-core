#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/client/messages.hpp>

namespace bts { namespace client { namespace detail {

bts::blockchain::transaction_id_type detail::client_impl::network_broadcast_transaction(const bts::blockchain::signed_transaction& transaction_to_broadcast)
{
   // ilog("broadcasting transaction: ${id} ", ("id", transaction_to_broadcast.id()));

   // p2p doesn't send messages back to the originator
   _p2p_node->broadcast(trx_message(transaction_to_broadcast));
   on_new_transaction(transaction_to_broadcast);
   return transaction_to_broadcast.id();
}

std::vector<fc::variant_object> detail::client_impl::network_get_peer_info( bool hide_firewalled_nodes )const
{
   std::vector<fc::variant_object> results;
   std::vector<bts::net::peer_status> peer_statuses = _p2p_node->get_connected_peers();
   for (const bts::net::peer_status& peer_status : peer_statuses)
      if(!hide_firewalled_nodes ||
            peer_status.info["firewall_status"].as_string() == "not_firewalled")
         results.push_back(peer_status.info);
   return results;
}

void detail::client_impl::network_set_allowed_peers(const vector<bts::net::node_id_t>& allowed_peers)
{
   _p2p_node->set_allowed_peers( allowed_peers );
}

void detail::client_impl::network_set_advanced_node_parameters(const fc::variant_object& params)
{
   _p2p_node->set_advanced_node_parameters( params );
}

fc::variant_object detail::client_impl::network_get_advanced_node_parameters() const
{
   return _p2p_node->get_advanced_node_parameters();
}

void detail::client_impl::network_add_node(const string& node, const string& command)
{
   if (command == "add")
      _self->connect_to_peer(node);
   else
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "unsupported command argument \"${command}\", valid commands are: \"add\"", ("command", command));
}

uint32_t detail::client_impl::network_get_connection_count() const
{
   return _p2p_node->get_connection_count();
}

bts::net::message_propagation_data detail::client_impl::network_get_transaction_propagation_data(const transaction_id_type& transaction_id)
{
   return _p2p_node->get_transaction_propagation_data(transaction_id);
   FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
}

bts::net::message_propagation_data detail::client_impl::network_get_block_propagation_data(const block_id_type& block_id)
{
   return _p2p_node->get_block_propagation_data(block_id);
   FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
}

fc::variant_object client_impl::network_get_info() const
{
   return _p2p_node->network_get_info();
}

fc::variant_object client_impl::network_get_usage_stats() const
{
   return _p2p_node->network_get_usage_stats();
}

vector<bts::net::potential_peer_record> client_impl::network_list_potential_peers()const
{
   return _p2p_node->get_potential_peers();
}

fc::variant_object client_impl::network_get_upnp_info()const
{
   fc::mutable_variant_object upnp_info;

   upnp_info["upnp_enabled"] = bool(_upnp_service);

   if (_upnp_service)
   {
      upnp_info["external_ip"] = fc::string(_upnp_service->external_ip());
      upnp_info["mapped_port"] = fc::variant(_upnp_service->mapped_port()).as_string();
   }

   return upnp_info;
}

} } } // namespace bts::client::detail
