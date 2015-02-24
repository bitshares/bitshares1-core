#include <fc/crypto/elliptic.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/network/ip.hpp>
#include <fstream>
#include <iostream>
#include <queue>

#include <bts/blockchain/chain_database.hpp>
#include <bts/net/peer_connection.hpp>
#include <bts/client/client.hpp>

class peer_probe : public bts::net::peer_connection_delegate
{
public:
  bool _peer_closed_connection;
  bool _we_closed_connection;
  bts::net::peer_connection_ptr _connection;
  std::vector<bts::net::address_info> _peers;
  fc::ecc::public_key _node_id;
  bool _connection_was_rejected;
  bool _done;
  fc::promise<void>::ptr _probe_complete_promise;

public:
  peer_probe() :
    _peer_closed_connection(false),
    _we_closed_connection(false),
    _connection(bts::net::peer_connection::make_shared(this)),
    _connection_was_rejected(false),
    _done(false),
    _probe_complete_promise(fc::promise<void>::ptr(new fc::promise<void>("probe_complete")))
  {}

  void start(const fc::ip::endpoint& endpoint_to_probe,
             const fc::ecc::private_key& my_node_id,
             const bts::blockchain::digest_type& chain_id)
  {
    fc::future<void> connect_task = fc::async([=](){ _connection->connect_to(endpoint_to_probe); }, "connect_task");
    try
    {
      connect_task.wait(fc::seconds(10));
    }
    catch (const fc::timeout_exception&)
    {
      ilog("timeout connecting to node ${endpoint}", ("endpoint", endpoint_to_probe));
      connect_task.cancel(__FUNCTION__);
      throw;
    }

    fc::sha256::encoder shared_secret_encoder;
    fc::sha512 shared_secret = _connection->get_shared_secret();
    shared_secret_encoder.write(shared_secret.data(), sizeof(shared_secret));
    fc::ecc::compact_signature signature = my_node_id.sign_compact(shared_secret_encoder.result());

    bts::net::hello_message hello("map_bts_network",
                                  BTS_NET_PROTOCOL_VERSION,
                                  fc::ip::address(), 0, 0,
                                  my_node_id.get_public_key(),
                                  signature,
                                  chain_id,
                                  fc::variant_object());

    _connection->send_message(hello);
  }

  void on_message(bts::net::peer_connection* originating_peer,
                  const bts::net::message& received_message) override
  {
    bts::net::message_hash_type message_hash = received_message.id();
    dlog( "handling message ${type} ${hash} size ${size} from peer ${endpoint}",
          ( "type", bts::net::core_message_type_enum(received_message.msg_type ) )("hash", message_hash )("size", received_message.size )("endpoint", originating_peer->get_remote_endpoint() ) );
    switch ( received_message.msg_type )
    {
    case bts::net::core_message_type_enum::hello_message_type:
      on_hello_message( originating_peer, received_message.as<bts::net::hello_message>() );
      break;
    case bts::net::core_message_type_enum::connection_accepted_message_type:
      on_connection_accepted_message( originating_peer, received_message.as<bts::net::connection_accepted_message>() );
      break;
    case bts::net::core_message_type_enum::connection_rejected_message_type:
      on_connection_rejected_message( originating_peer, received_message.as<bts::net::connection_rejected_message>() );
      break;
    case bts::net::core_message_type_enum::address_request_message_type:
      on_address_request_message( originating_peer, received_message.as<bts::net::address_request_message>() );
      break;
    case bts::net::core_message_type_enum::address_message_type:
      on_address_message( originating_peer, received_message.as<bts::net::address_message>() );
      break;
    case bts::net::core_message_type_enum::closing_connection_message_type:
      on_closing_connection_message( originating_peer, received_message.as<bts::net::closing_connection_message>() );
      break;
    case bts::net::core_message_type_enum::current_time_request_message_type:
      on_current_time_request_message( originating_peer, received_message.as<bts::net::current_time_request_message>() );
      break;
    case bts::net::core_message_type_enum::current_time_reply_message_type:
      on_current_time_reply_message( originating_peer, received_message.as<bts::net::current_time_reply_message>() );
      break;
    }
  }

  void on_hello_message(bts::net::peer_connection* originating_peer,
                        const bts::net::hello_message& hello_message_received)
  {
    _node_id = hello_message_received.node_public_key;
    if (hello_message_received.user_data.contains("node_id"))
      originating_peer->node_id = hello_message_received.user_data["node_id"].as<bts::net::node_id_t>();
    originating_peer->send_message(bts::net::connection_rejected_message());
  }

  void on_connection_accepted_message(bts::net::peer_connection* originating_peer,
                                      const bts::net::connection_accepted_message& connection_accepted_message_received)
  {
    _connection_was_rejected = false;
    originating_peer->send_message(bts::net::address_request_message());
  }

  void on_connection_rejected_message( bts::net::peer_connection* originating_peer,
                                       const bts::net::connection_rejected_message& connection_rejected_message_received )
  {
    _connection_was_rejected = true;
    originating_peer->send_message(bts::net::address_request_message());
  }

  void on_address_request_message(bts::net::peer_connection* originating_peer,
                                  const bts::net::address_request_message& address_request_message_received)
  {
    originating_peer->send_message(bts::net::address_message());
  }


  void on_address_message(bts::net::peer_connection* originating_peer,
                          const bts::net::address_message& address_message_received)
  {
    _peers = address_message_received.addresses;
    originating_peer->send_message(bts::net::closing_connection_message("Thanks for the info"));
    _we_closed_connection = true;
  }

  void on_closing_connection_message(bts::net::peer_connection* originating_peer,
                                     const bts::net::closing_connection_message& closing_connection_message_received)
  {
    if (_we_closed_connection)
      _connection->close_connection();
    else
      _peer_closed_connection = true;
  }

  void on_current_time_request_message(bts::net::peer_connection* originating_peer,
                                       const bts::net::current_time_request_message& current_time_request_message_received)
  {
  }

  void on_current_time_reply_message(bts::net::peer_connection* originating_peer,
                                     const bts::net::current_time_reply_message& current_time_reply_message_received)
  {
  }

  void on_connection_closed(bts::net::peer_connection* originating_peer) override
  {
    _done = true;
    _probe_complete_promise->set_value();
  }

  bts::net::message get_message_for_item(const bts::net::item_id& item) override
  {
    return bts::net::item_not_available_message(item);
  }

  void wait()
  {
    _probe_complete_promise->wait();
  }
};

int main(int argc, char** argv)
{
  std::queue<fc::ip::endpoint> nodes_to_visit;
  std::set<fc::ip::endpoint> nodes_to_visit_set;
  std::set<fc::ip::endpoint> nodes_already_visited;

  fc::path data_dir = fc::temp_directory_path() / "map_bts_network";
  fc::create_directories(data_dir);

  bts::client::config default_client_config;
  for (const std::string default_peer : default_client_config.default_peers)
    nodes_to_visit.push(fc::ip::endpoint::from_string(default_peer));
  fc::ip::endpoint seed_node1 = nodes_to_visit.front();

  fc::ecc::private_key my_node_id = fc::ecc::private_key::generate();
  bts::blockchain::chain_database_ptr chain_db = std::make_shared<bts::blockchain::chain_database>();
  chain_db->open(data_dir / "chain", fc::optional<fc::path>("C:/Users/Administrator/AppData/Local/Temp/map_bts_network/genesis.json"), true);

  std::map<bts::net::node_id_t, bts::net::address_info> address_info_by_node_id;
  std::map<bts::net::node_id_t, std::vector<bts::net::address_info> > connections_by_node_id;
  //std::map<bts::net::node_id_t, fc::ip::endpoint> all_known_nodes;

  while (!nodes_to_visit.empty())
  {
    bts::net::address_info this_node_info;
    this_node_info.direction = bts::net::peer_connection_direction::outbound;
    this_node_info.firewalled = bts::net::firewalled_state::not_firewalled;

    this_node_info.remote_endpoint = nodes_to_visit.front();;
    nodes_to_visit.pop();
    nodes_to_visit_set.erase(this_node_info.remote_endpoint);
    nodes_already_visited.insert(this_node_info.remote_endpoint);

    peer_probe probe;
    try
    {
      probe.start(this_node_info.remote_endpoint,
                  my_node_id,
                  chain_db->get_chain_id());
      probe.wait();

      this_node_info.node_id = probe._node_id;

      connections_by_node_id[this_node_info.node_id] = probe._peers;
      if (address_info_by_node_id.find(probe._node_id) == address_info_by_node_id.end())
        address_info_by_node_id[probe._node_id] = this_node_info;

      for (const bts::net::address_info& info : probe._peers)
      {
        if (nodes_already_visited.find(info.remote_endpoint) == nodes_already_visited.end() &&
            info.firewalled == bts::net::firewalled_state::not_firewalled &&
            nodes_to_visit_set.find(info.remote_endpoint) == nodes_to_visit_set.end())
        {
          nodes_to_visit.push(info.remote_endpoint);
          nodes_to_visit_set.insert(info.remote_endpoint);
        }
        if (address_info_by_node_id.find(info.node_id) == address_info_by_node_id.end())
          address_info_by_node_id[info.node_id] = info;
      }
    }
    catch (const fc::exception&)
    {
    }
    std::cout << "Traversed " << nodes_already_visited.size() << " of " << (nodes_already_visited.size() + nodes_to_visit.size()) << " known nodes\n";
  }

  bts::net::node_id_t seed_node_id;
  std::set<bts::net::node_id_t> non_firewalled_nodes_set;
  for (const auto& address_info_for_node : address_info_by_node_id)
  {
    if (address_info_for_node.second.remote_endpoint == seed_node1)
      seed_node_id = address_info_for_node.first;
    if (address_info_for_node.second.firewalled == bts::net::firewalled_state::not_firewalled)
      non_firewalled_nodes_set.insert(address_info_for_node.first);
  }
  std::set<bts::net::node_id_t> seed_node_connections;
  for (const bts::net::address_info& info : connections_by_node_id[seed_node_id])
    seed_node_connections.insert(info.node_id);
  std::set<bts::net::node_id_t> seed_node_missing_connections;
  std::set_difference(non_firewalled_nodes_set.begin(), non_firewalled_nodes_set.end(),
                      seed_node_connections.begin(), seed_node_connections.end(),
                      std::inserter(seed_node_missing_connections, seed_node_missing_connections.end()));
  seed_node_missing_connections.erase(seed_node_id);

  std::ofstream dot_stream((data_dir / "network_graph.dot").string().c_str());
  std::map<bts::net::node_id_t, fc::ip::endpoint> all_known_nodes;

  dot_stream << "graph G {\n";
  dot_stream << "  // Total " << address_info_by_node_id.size() << " nodes, firewalled: " << (address_info_by_node_id.size() - non_firewalled_nodes_set.size())
                              << ", non-firewalled: " << non_firewalled_nodes_set.size() << "\n";
  dot_stream << "  // Seed node is " << (std::string)address_info_by_node_id[seed_node_id].remote_endpoint << " id: " << fc::variant(seed_node_id).as_string() << "\n";
  dot_stream << "  // Seed node is connected to " << connections_by_node_id[seed_node_id].size() << " nodes\n";
  dot_stream << "  // Seed node is missing connections to " << seed_node_missing_connections.size() << " non-firewalled nodes:\n";
  for (const bts::net::node_id_t& id : seed_node_missing_connections)
    dot_stream << "  //           " << (std::string)address_info_by_node_id[id].remote_endpoint << "\n";

  dot_stream << "  layout=\"circo\";\n";
  //for (const auto& node_and_connections : connections_by_node_id)
  //  all_known_nodes[node_and_connections.first] = address_info_by_node_id[node_and_connections.first].remote_endpoint;

  for (const auto& address_info_for_node : address_info_by_node_id)
  {
    dot_stream << "  \"" << fc::variant(address_info_for_node.first).as_string() << "\"[label=\"" << (std::string)address_info_for_node.second.remote_endpoint << "\"";
    if (address_info_for_node.second.firewalled != bts::net::firewalled_state::not_firewalled)
      dot_stream << ",shape=rectangle";
    dot_stream << "];\n";
  }
  for (auto& node_and_connections : connections_by_node_id)
    for (const bts::net::address_info& this_connection : node_and_connections.second)
      dot_stream << "  \"" << fc::variant(node_and_connections.first).as_string() << "\" -- \"" << fc::variant(this_connection.node_id).as_string() << "\";\n";

  dot_stream << "}\n";

#if 0
  for (auto& node_and_connections : connections_by_node_id)
  {
    out << "  " << (std::string)node_and_connections.first.node_id.data << "[label=\"" << (std::string)node_and_connections.first.remote_endpoint << "\"];\n";
    //node_and_connections.first.node_id
  }
#endif

  return 0;
}
