#define BOOST_TEST_MODULE BtsXtClientTests
#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>
#include <boost/algorithm/string/join.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/exterior_property.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>
#include <boost/graph/eccentricity.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/copy.hpp>

#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/json.hpp>

#include <fc/interprocess/process.hpp>

#include <fc/reflect/variant.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/net/chain_server.hpp>


////////////////////////////////////////////////////////////////////////////////
// Parse the command line for configuration options here ///////////////////////
struct bts_xt_client_test_config
{
  static fc::path bts_client_exe;
  static fc::path bts_server_exe;
  static fc::path config_directory;
  static uint16_t base_rpc_port;
  static uint16_t base_p2p_port;
  static uint16_t base_http_port;
  static bool test_client_server;

  bts_xt_client_test_config()
  {
    // parse command-line options
    boost::program_options::options_description option_config("Allowed options");
    option_config.add_options()("bts-client-exe", boost::program_options::value<std::string>(), "full path to the executable to test")
                               ("bts-server-exe", boost::program_options::value<std::string>(), "full path to the server executable for testing client-server mode")
                               ("client-server", "test client-server mode instead of p2p")
                               ("extra-help", "display this help message");


    boost::program_options::variables_map option_variables;
    try
    {
      boost::program_options::store(boost::program_options::command_line_parser(boost::unit_test::framework::master_test_suite().argc,
                                                                                boost::unit_test::framework::master_test_suite().argv).
        options(option_config).run(), option_variables);
      boost::program_options::notify(option_variables);
    }
    catch (boost::program_options::error&)
    {
      std::cerr << "Error parsing command-line options\n\n";
      std::cerr << option_config << "\n";
      exit(1);
    }

    if (option_variables.count("extra-help"))
    {
      std::cout << option_config << "\n";
      exit(0);
    }

    if (option_variables.count("bts-client-exe"))
      bts_client_exe = option_variables["bts-client-exe"].as<std::string>().c_str();

    if (option_variables.count("bts-server-exe"))
      bts_server_exe = option_variables["bts-server-exe"].as<std::string>().c_str();

    if (option_variables.count("client-server"))
      test_client_server = true;

    std::cout << "Testing " << bts_client_exe.string() << "\n";
    std::cout << "Using config directory " << config_directory.string() << "\n";
    fc::create_directories(config_directory);

    boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
  }
};
fc::path bts_xt_client_test_config::bts_client_exe = "e:/invictus/vs12_bt/programs/bts_xt/Debug/bts_xt_client.exe";
fc::path bts_xt_client_test_config::bts_server_exe = "e:/invictus/vs12_bt/programs/bts_xt/Debug/bts_xt_server.exe";
fc::path bts_xt_client_test_config::config_directory = fc::temp_directory_path() / "bts_xt_client_tests";
uint16_t bts_xt_client_test_config::base_rpc_port  = 20100;
uint16_t bts_xt_client_test_config::base_p2p_port  = 21100;
uint16_t bts_xt_client_test_config::base_http_port = 22100;
bool bts_xt_client_test_config::test_client_server = false;

#define RPC_USERNAME "test"
#define RPC_PASSWORD "test"
#define WALLET_PASPHRASE "testtest"
#define INITIAL_BALANCE 100000000

BOOST_GLOBAL_FIXTURE(bts_xt_client_test_config);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// class to log stdio and shut down the controlled process when destructed
struct managed_process
{
  fc::process_ptr process;
  fc::future<void> stdout_reader_done;
  fc::future<void> stderr_reader_done;

  ~managed_process();
  void log_stdout_stderr_to_file(const fc::path& logfile);
};

managed_process::~managed_process()
{
  if (process)
  {
    process->kill();
    if (stdout_reader_done.valid() && !stdout_reader_done.ready())
    {
      try
      {
        stdout_reader_done.wait();
      }
      catch (fc::exception&)
      {}
    }
    if (stderr_reader_done.valid() && !stderr_reader_done.ready())
    {
      try
      {
        stderr_reader_done.wait();
      }
      catch (fc::exception&)
      {}
    }
  }
}

void managed_process::log_stdout_stderr_to_file(const fc::path& logfile)
{
  std::shared_ptr<std::ofstream> stdouterrfile = std::make_shared<std::ofstream>(logfile.string().c_str());
  fc::buffered_istream_ptr out_stream = process->out_stream();
  fc::buffered_istream_ptr err_stream = process->err_stream();
  stdout_reader_done = fc::async([out_stream,stdouterrfile]()
    {
      char buf[1024];
      for (;;)
      {
        size_t bytes_read = out_stream->readsome(buf, sizeof(buf));
        if (!bytes_read)
          break;
        stdouterrfile->write(buf, bytes_read);
        stdouterrfile->flush();
      }
    });
  stderr_reader_done = fc::async([err_stream,stdouterrfile]()
    {
      char buf[1024];
      for (;;)
      {
        size_t bytes_read = err_stream->readsome(buf, sizeof(buf));
        if (!bytes_read)
          break;
        stdouterrfile->write(buf, bytes_read);
        stdouterrfile->flush();
      }
    });
}

struct bts_server_process : managed_process
{
  void launch(const bts::net::genesis_block_config& genesis_block, const fc::ecc::private_key& trustee_key);
};
typedef std::shared_ptr<bts_server_process> bts_server_process_ptr;

void bts_server_process::launch(const bts::net::genesis_block_config& genesis_block,
                                const fc::ecc::private_key& trustee_key)
{
  process = std::make_shared<fc::process>();

  std::vector<std::string> options;
  options.push_back("--trustee-address");
  options.push_back(bts::blockchain::address(trustee_key.get_public_key()));

  fc::path server_config_dir = bts_xt_client_test_config::config_directory / "BitSharesX_Server";
  fc::remove_all(server_config_dir);
  fc::create_directories(server_config_dir);

  fc::json::save_to_file(genesis_block, server_config_dir / "genesis.json", true);

  process->exec(bts_xt_client_test_config::bts_server_exe, options, server_config_dir);

  {
#ifdef _MSC_VER
    std::ofstream launch_script((server_config_dir / "launch.bat").string());
#else
    std::ofstream launch_script((server_config_dir / "launch.sh").string());;
#endif
    launch_script << bts_xt_client_test_config::bts_server_exe.string() << " " << boost::algorithm::join(options, " ") << "\n";
  }


  log_stdout_stderr_to_file(server_config_dir / "stdouterr.txt");
}

//////////////////////////////////////////////////////////////////////////////////////////

struct bts_client_process : managed_process
{
  uint64_t initial_balance;
  fc::ecc::private_key private_key;
  uint16_t rpc_port;
  uint16_t p2p_port;
  uint16_t http_port;
  bts::rpc::rpc_client_ptr rpc_client;
  fc::uint160_t node_id;

  bts_client_process() : initial_balance(0) {}
  void launch(uint32_t process_number,
              const fc::ecc::private_key& trustee_key,
              bool act_as_trustee,
              fc::optional<bts::net::genesis_block_config> genesis_block);
};
typedef std::shared_ptr<bts_client_process> bts_client_process_ptr;

void bts_client_process::launch(uint32_t process_number,
                                const fc::ecc::private_key& trustee_key,
                                bool act_as_trustee,
                                fc::optional<bts::net::genesis_block_config> genesis_block)
{
  process = std::make_shared<fc::process>();
  std::vector<std::string> options;

  std::ostringstream numbered_config_dir_name;
  numbered_config_dir_name << "BitSharesX_" << std::setw(3) << std::setfill('0') << process_number;
  fc::path numbered_config_dir = bts_xt_client_test_config::config_directory / numbered_config_dir_name.str();
  fc::remove_all(numbered_config_dir);
  fc::create_directories(numbered_config_dir);

  // create a wallet in that directory
  // we could (and probably should) make bts_xt_client create the wallet,
  // but if we ask it to create the wallet
  // it will interactively prompt for passwords which is a big hassle.
  // here we explicitly create one with a blank password
  {
    bts::wallet::wallet_ptr wallet = std::make_shared<bts::wallet::wallet>();
    wallet->set_data_directory(numbered_config_dir);
    wallet->create("default", "", WALLET_PASPHRASE);
  }

  options.push_back("--data-dir");
  options.push_back(numbered_config_dir.string());

  options.push_back("--server");
  options.push_back("--rpcuser=" RPC_USERNAME);
  options.push_back("--rpcpassword=" RPC_PASSWORD);
  options.push_back("--rpcport");
  options.push_back(boost::lexical_cast<std::string>(rpc_port));
  options.push_back("--httpport");
  options.push_back(boost::lexical_cast<std::string>(http_port));
  options.push_back("--trustee-address");
  options.push_back(bts::blockchain::address(trustee_key.get_public_key()));
  if (act_as_trustee)
  {
    options.push_back("--trustee-private-key");
    options.push_back(trustee_key.get_secret());
  }
  if (!bts_xt_client_test_config::test_client_server)
  {
    options.push_back("--p2p");
    options.push_back("--port");
    options.push_back(boost::lexical_cast<std::string>(p2p_port));
    /* options.push_back("--connect-to");
    options.push_back(std::string("127.0.0.1:") + boost::lexical_cast<std::string>(bts_xt_client_test_config::base_p2p_port)); */
  }
  if (genesis_block)
  {
    fc::path genesis_json(numbered_config_dir / "genesis.json");
    fc::json::save_to_file(*genesis_block, genesis_json, true);
    options.push_back("--genesis-json");
    options.push_back(genesis_json.string());
  }

  {
#ifdef _MSC_VER
    std::ofstream launch_script((numbered_config_dir / "launch.bat").string());
#else
    std::ofstream launch_script((numbered_config_dir / "launch.sh").string());;
#endif
    launch_script << bts_xt_client_test_config::bts_client_exe.string() << " " << boost::algorithm::join(options, " ") << "\n";
  }

  process->exec(bts_xt_client_test_config::bts_client_exe, options, numbered_config_dir);

  log_stdout_stderr_to_file(numbered_config_dir / "stdouterr.txt");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct bts_client_launcher_fixture
{
  bts_server_process_ptr server_process;
  std::vector<bts_client_process> client_processes;

  bts::net::genesis_block_config genesis_block;
  fc::ecc::private_key trustee_key = fc::ecc::private_key::generate();

  uint32_t _peer_connection_retry_timeout;
  uint32_t _desired_number_of_connections;
  uint32_t _maximum_number_of_connections;

  typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> DirectedGraph;
  DirectedGraph _directed_graph;
  typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> UndirectedGraph;
  UndirectedGraph _undirected_graph;

  bts_client_launcher_fixture() :
    _peer_connection_retry_timeout(15 /* sec */),
    _desired_number_of_connections(3),
    _maximum_number_of_connections(5)
  {}

  //const uint32_t test_process_count = 10;

  void create_trustee_and_genesis_block();
  void launch_server();
  void launch_clients();
  void establish_rpc_connections();
  void trigger_network_connections();
  void import_initial_balances();
  int verify_network_connectivity(const fc::path& output_path);
  void get_node_ids();
  void create_propagation_graph(const std::vector<bts::net::message_propagation_data>& propagation_data, int initial_node, const fc::path& output_file);
};

void bts_client_launcher_fixture::create_trustee_and_genesis_block()
{
  // generate a genesis block giving 100bts to each account
  BOOST_TEST_MESSAGE("Generating keys for " << client_processes.size() << " clients");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].private_key = fc::ecc::private_key::generate();
    genesis_block.balances.push_back(std::make_pair(bts::blockchain::pts_address(client_processes[i].private_key.get_public_key()),
                                                    client_processes[i].initial_balance / 100000000));
  }

  BOOST_TEST_MESSAGE("Generating trustee keypair");
  trustee_key = fc::ecc::private_key::generate();
}

void bts_client_launcher_fixture::launch_server()
{
  BOOST_TEST_MESSAGE("Launching bts_xt_server processe");
  server_process = std::make_shared<bts_server_process>();
  server_process->launch(genesis_block, trustee_key);
}

void bts_client_launcher_fixture::launch_clients()
{
  BOOST_TEST_MESSAGE("Launching " << client_processes.size() << " bts_xt_client processes");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_port = bts_xt_client_test_config::base_rpc_port + i;
    client_processes[i].p2p_port = bts_xt_client_test_config::base_p2p_port + i;
    client_processes[i].http_port = bts_xt_client_test_config::base_http_port + i;
    fc::optional<bts::net::genesis_block_config> optional_genesis_block;
#if 1
    // simulate the condition where some clients start without a genesis block shipped to them
    if (i == 0 && !bts_xt_client_test_config::test_client_server)
      optional_genesis_block = genesis_block;
#else
    // simulate the condition where all clients and all servers start with a genesis block
    // this doesn't currently work, because the initial block contains a timestamp and each
    // client will generate a different genesis block
    optional_genesis_block = genesis_block;
#endif
    client_processes[i].launch(i, trustee_key, i == 0,
                               optional_genesis_block);
  }
}

void bts_client_launcher_fixture::establish_rpc_connections()
{
  fc::usleep(fc::seconds(10));


  BOOST_TEST_MESSAGE("Establishing JSON-RPC connections to all processes");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client = std::make_shared<bts::rpc::rpc_client>();
    client_processes[i].rpc_client->connect_to(fc::ip::endpoint(fc::ip::address("127.0.0.1"), client_processes[i].rpc_port));
  }

  BOOST_TEST_MESSAGE("Logging in to JSON-RPC connections");
  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].rpc_client->login(RPC_USERNAME, RPC_PASSWORD);
}

void bts_client_launcher_fixture::trigger_network_connections()
{
  BOOST_TEST_MESSAGE("Triggering network connections between active processes");

  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    if (bts_xt_client_test_config::test_client_server)
    {
      //client_processes[i].rpc_client->addnode(fc::ip::endpoint(fc::ip::address("127.0.0.1"), bts_xt_client_test_config::base_p2p_port), "add");
    }
    else //test p2p
    {
      fc::mutable_variant_object parameters;
      parameters["peer_connection_retry_timeout"] = _peer_connection_retry_timeout; // seconds
      parameters["desired_number_of_connections"] = _desired_number_of_connections;
      parameters["maximum_number_of_connections"] = _maximum_number_of_connections;
      client_processes[i].rpc_client->_set_advanced_node_parameters(parameters);
      client_processes[i].rpc_client->addnode(fc::ip::endpoint(fc::ip::address("127.0.0.1"), bts_xt_client_test_config::base_p2p_port), "add");
      fc::usleep(fc::milliseconds(250));
    }
  }
}

void bts_client_launcher_fixture::import_initial_balances()
{
  BOOST_TEST_MESSAGE("Importing initial keys and verifying initial balances");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->import_private_key(client_processes[i].private_key.get_secret(), "blah");
    client_processes[i].rpc_client->rescan(0);
    BOOST_REQUIRE_EQUAL(client_processes[i].rpc_client->getbalance(0).amount, client_processes[i].initial_balance);
  }
}

int bts_client_launcher_fixture::verify_network_connectivity(const fc::path& output_path)
{
  fc::create_directories(output_path);

  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    fc::variants peers_info = client_processes[i].rpc_client->getpeerinfo();
    for (const fc::variant& peer_info : peers_info)
    {
      fc::variant_object peer_info_object = peer_info.get_object();
      if (peer_info_object["inbound"].as_bool())
      {
        uint16_t peer_p2p_port = fc::ip::endpoint::from_string(peer_info_object["addr"].as_string()).port();
        boost::add_edge(peer_p2p_port - bts_xt_client_test_config::base_p2p_port, i, _directed_graph);
      }
    }
    BOOST_CHECK(peers_info.size() >= _desired_number_of_connections);
    BOOST_CHECK(peers_info.size() <= _maximum_number_of_connections);
  }

  unsigned number_of_partitions = 0;

  if (boost::num_vertices(_directed_graph))
  {
    typedef boost::exterior_vertex_property<UndirectedGraph, int> DistanceProperty;
    typedef DistanceProperty::matrix_type DistanceMatrix;
    typedef DistanceProperty::matrix_map_type DistanceMatrixMap;
    typedef boost::graph_traits<UndirectedGraph>::edge_descriptor Edge;
    typedef boost::constant_property_map<Edge, int> WeightMap;

    boost::copy_graph(_directed_graph, _undirected_graph);

    std::vector<int> component(boost::num_vertices(_undirected_graph));
    number_of_partitions = boost::connected_components(_undirected_graph, &component[0]);
    BOOST_CHECK(number_of_partitions == 1);
    if (number_of_partitions != 1)
    {
      std::vector<std::vector<int> > nodes_by_component(number_of_partitions);
      for (unsigned i = 0; i < component.size(); ++i)
        nodes_by_component[component[i]].push_back(i);
      BOOST_TEST_MESSAGE("Network is partitioned into " << number_of_partitions<< " disconnected groups");
      for (unsigned i = 0; i < number_of_partitions; ++i)
      {
        std::ostringstream nodes;
        for (unsigned j = 0; j < nodes_by_component[i].size(); ++j)
          nodes << " " << nodes_by_component[i][j];
        BOOST_TEST_MESSAGE("Partition " << i << ":" << nodes.str());
      }
    }

    DistanceMatrix distances(boost::num_vertices(_undirected_graph));
    DistanceMatrixMap distance_map(distances, _undirected_graph);
    WeightMap weight_map(1);
    boost::floyd_warshall_all_pairs_shortest_paths(_undirected_graph, distance_map, boost::weight_map(weight_map));

    uint32_t longest_path_source = 0;
    uint32_t longest_path_destination = 0;
    int longest_path_length = 0;
    for (uint32_t i = 0; i < boost::num_vertices(_undirected_graph); ++i)
      for (uint32_t j = 0; j < boost::num_vertices(_undirected_graph); ++j)
      {
        assert(distances[i][j] == distances[j][i]);
        if (distances[i][j] > longest_path_length && distances[i][j] != std::numeric_limits<WeightMap::value_type>::max())
        {
          longest_path_length = distances[i][j];
          longest_path_source = i;
          longest_path_destination = j;
        }
      }

    typedef boost::exterior_vertex_property<UndirectedGraph, int> EccentricityProperty;
    typedef EccentricityProperty::container_type EccentricityContainer;
    typedef EccentricityProperty::map_type EccentricityMap;
    int radius, diameter;
    EccentricityContainer eccentricity_container(boost::num_vertices(_undirected_graph));
    EccentricityMap eccentricity_map(eccentricity_container, _undirected_graph);
    boost::tie(radius, diameter) = boost::all_eccentricities(_undirected_graph, distance_map, eccentricity_map);

    // write out a graph for visualization
    struct graph_property_writer
    {
      void operator()(std::ostream& out) const
      {
        out << "overlap = false;\n";
        out << "splines = true;\n";
        out << "node [shape = circle, fontname = Helvetica, fontsize = 10]\n";
      }
    };

    fc::path dot_file_path = output_path / "network_map.dot";
    std::ofstream dot_file(dot_file_path.string());
    dot_file << "// Graph radius is " << radius << ", diameter is " << diameter << "\n";
    dot_file << "// Longest path is from " << longest_path_source << " to " << longest_path_destination << ", length " << longest_path_length << "\n";
    dot_file << "//   node parameters: desired_connections: " << _desired_number_of_connections
             << ", max connections: " << _maximum_number_of_connections << "\n";
    boost::write_graphviz(dot_file, _directed_graph, boost::default_writer(), boost::default_writer(), graph_property_writer());
    dot_file.close();
  } // end if num_vertices != 0

  return number_of_partitions;
}

void bts_client_launcher_fixture::get_node_ids()
{
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    BOOST_CHECK_NO_THROW(client_processes[i].node_id = client_processes[i].rpc_client->getinfo()["_node_id"].as<fc::uint160_t>());
  }
}

void bts_client_launcher_fixture::create_propagation_graph(const std::vector<bts::net::message_propagation_data>& propagation_data, int initial_node, const fc::path& output_file)
{
  struct graph_property_writer
  {
    void operator()(std::ostream& out) const
    {
      out << "overlap = false;\n";
      out << "splines = true;\n";
      out << "edge [fontname = Helvetica, fontsize = 8]\n";
      out << "node [shape = oval, fontname = Helvetica, fontsize = 10, style=filled, colorscheme=bugn9]\n";
    }
  };

  fc::time_point min_time = fc::time_point::maximum();
  fc::time_point max_time = fc::time_point::min();
  for (unsigned i = 0; i < propagation_data.size(); ++i)
  {
    min_time = std::min(min_time, propagation_data[i].received_time);
    max_time = std::max(max_time, propagation_data[i].received_time);
  }
  fc::microseconds max_duration = max_time - min_time;

  std::map<fc::uint160_t, int> node_id_to_node_map;
  for (unsigned i = 0; i < client_processes.size(); ++i)
    node_id_to_node_map[client_processes[i].node_id] = i;

  class vertex_label_writer {
  public:
    vertex_label_writer(const std::vector<bts::net::message_propagation_data>& propagation_data, int initial_node, fc::microseconds max_duration) :
      _propagation_data(propagation_data),
      _initial_node(initial_node),
      _max_duration(max_duration)
    {}
      
    void operator()(std::ostream& out, int v) const {
      fc::microseconds this_duration = _propagation_data[v].received_time - _propagation_data[_initial_node].received_time;
      int color = (int)((7 * this_duration.count()) / _max_duration.count()) + 1; // map to colors 1 - 8 out of 9, 9 is too dark
      out << "[label=<" << v << "<br/>" << this_duration.count() / 1000 << "ms>, fillcolor=" << color;
      if (v == _initial_node)
        out << ", shape=doublecircle";
      out << "]";
    }
  private:
    const std::vector<bts::net::message_propagation_data>& _propagation_data;
    int _initial_node;
    fc::microseconds _max_duration;
  };

  class edge_label_writer {
  public:
    edge_label_writer(const std::vector<bts::net::message_propagation_data>& propagation_data, 
                      int initial_node, 
                      fc::microseconds max_duration,
                      DirectedGraph& directed_graph,
                      std::map<fc::uint160_t, int> node_id_to_node_map) :
      _propagation_data(propagation_data),
      _initial_node(initial_node),
      _max_duration(max_duration),
      _directed_graph(directed_graph),
      _node_id_to_node_map(node_id_to_node_map)
    {}
      
    void operator()(std::ostream& out, const boost::graph_traits<DirectedGraph>::edge_descriptor& e) const {
      int target = boost::target(e, _directed_graph);
      int source = boost::source(e, _directed_graph);
      bool swapped = false;

      auto iter = _node_id_to_node_map.find(_propagation_data[source].originating_peer);
      if (iter != _node_id_to_node_map.end() && 
          iter->second == target)
      {
        std::swap(target, source);
        swapped = true;
      }

      iter = _node_id_to_node_map.find(_propagation_data[target].originating_peer);
      if (iter != _node_id_to_node_map.end() && 
          iter->second == source)
      {
        fc::microseconds path_delay = _propagation_data[target].received_time - _propagation_data[source].received_time;
        out << "[label=\"" << path_delay.count() / 1000 << "ms\"";
        if (swapped)
          out << "dir=back";
        out << "]";
      }
      else
      {
        out << "[style=dotted arrowhead=none]";
      } 
      //fc::microseconds this_duration = _propagation_data[v].received_time - _propagation_data[_initial_node].received_time;
      //int color = ((7 * this_duration.count()) / _max_duration.count()) + 1; // map to colors 1 - 8 out of 9, 9 is too dark
      //out << "[label=<" << v << "<br/>" << this_duration.count() << "us>, fillcolor=" << color;
      //if (v == _initial_node)
      //  out << ", shape=doublecircle";
      //out << "]";
    }
  private:
    const std::vector<bts::net::message_propagation_data>& _propagation_data;
    int _initial_node;
    fc::microseconds _max_duration;
    DirectedGraph& _directed_graph;
    std::map<fc::uint160_t, int> _node_id_to_node_map;
  };

  fc::create_directories(output_file.parent_path());
  std::ofstream dot_file(output_file.string());
  boost::write_graphviz(dot_file, _directed_graph, 
                        vertex_label_writer(propagation_data, initial_node, max_duration),
                        edge_label_writer(propagation_data, initial_node, max_duration, _directed_graph, node_id_to_node_map), 
                        graph_property_writer());
}


BOOST_FIXTURE_TEST_SUITE(bts_xt_client_test_suite, bts_client_launcher_fixture)

#if 0
// test whether we can cancel a fc task that's sleeping (hint: we can't)
BOOST_AUTO_TEST_CASE(sleep_test)
{
  fc::future<void> ten = fc::async([](){ fc::usleep(fc::seconds(300)); std::cout << "300 second sleep completed\n"; });
  fc::usleep(fc::milliseconds(100));
  ten.cancel();
  ten.wait();
  fc::future<void> five = fc::async([](){ fc::usleep(fc::seconds(5)); std::cout << "5 second sleep completed\n"; });
  five.wait();
}
#endif


BOOST_AUTO_TEST_CASE(standalone_wallet_test)
{
  client_processes.resize(1);

  client_processes[0].initial_balance = 100000000; // not important, we just need a nonzero balance to avoid crashing

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();
  trigger_network_connections();

#if 0
  BOOST_TEST_MESSAGE("Testing a wallet operation without logging in");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    BOOST_CHECK_THROW(client_processes[i].rpc_client->getbalance(0), fc::exception)
  }
#endif

  BOOST_TEST_MESSAGE("Testing open_wallet() call");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
  }

  BOOST_TEST_MESSAGE("Verifying all clients have zero balance after opening wallet");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    bts::blockchain::asset balance = client_processes[i].rpc_client->getbalance(0);
    BOOST_CHECK(balance == bts::blockchain::asset());
  }

  BOOST_TEST_MESSAGE("Testing unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    BOOST_CHECK_THROW(client_processes[i].rpc_client->walletpassphrase("this is not the correct wallet passphrase", fc::seconds(60)), fc::exception)
    BOOST_CHECK_NO_THROW(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  BOOST_TEST_MESSAGE("Testing receive address generation");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    std::unordered_set<bts::wallet::receive_address> initial_addresses = client_processes[i].rpc_client->list_receive_addresses();
    BOOST_CHECK(initial_addresses.empty());
    std::string addressMemo("address_test_account");
    bts::blockchain::address new_address = client_processes[i].rpc_client->getnewaddress(addressMemo);
    BOOST_CHECK(initial_addresses.find(new_address) == initial_addresses.end());
    std::unordered_set<bts::wallet::receive_address> final_addresses = client_processes[i].rpc_client->list_receive_addresses();
    BOOST_CHECK(final_addresses.size() == initial_addresses.size() + 1);
    for (auto value : initial_addresses)
    {
      BOOST_CHECK(final_addresses.find(value) != final_addresses.end());
    }
    BOOST_CHECK(final_addresses.find(new_address) != final_addresses.end());
    BOOST_CHECK(final_addresses.find(new_address)->memo == addressMemo);
  }
}

BOOST_AUTO_TEST_CASE(unlocking_test)
{
  client_processes.resize(1);

  client_processes[0].initial_balance = 100000000; // not important, we just need a nonzero balance to avoid crashing

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();
  trigger_network_connections();


  client_processes[0].rpc_client->open_wallet();

  BOOST_TEST_MESSAGE("Testing getnewaddress() while wallet is locked");
  BOOST_CHECK_THROW(client_processes[0].rpc_client->getnewaddress(), fc::exception);
  BOOST_TEST_MESSAGE("Unlocking wallet for 1 second");
  client_processes[0].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::seconds(1));
  BOOST_TEST_MESSAGE("Testing getnewaddress() with wallet unlocked locked");
  BOOST_CHECK_NO_THROW(client_processes[0].rpc_client->getnewaddress());
  fc::usleep(fc::seconds(2));
  BOOST_TEST_MESSAGE("Testing getnewaddress() after wallet should have relocked");
  BOOST_CHECK_THROW(client_processes[0].rpc_client->getnewaddress(), fc::exception);

  BOOST_TEST_MESSAGE("Testing whether a second unlock cancels the first unlock");
  client_processes[0].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::seconds(4));
  client_processes[0].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::seconds(2));
  BOOST_TEST_MESSAGE("Testing getnewaddress immediately after both unlocks");
  BOOST_CHECK_NO_THROW(client_processes[0].rpc_client->getnewaddress());
  fc::usleep(fc::seconds(3));
  BOOST_TEST_MESSAGE("Testing getnewaddress after the second unlock expired, but first should still be in effect");
  BOOST_CHECK_NO_THROW(client_processes[0].rpc_client->getnewaddress());
  fc::usleep(fc::seconds(2));
  BOOST_TEST_MESSAGE("Testing that we correctly relock after both unlocks should have expired");
  BOOST_CHECK_THROW(client_processes[0].rpc_client->getnewaddress(), fc::exception);
}

BOOST_AUTO_TEST_CASE(transfer_test)
{
  client_processes.resize(5);

  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].initial_balance = INITIAL_BALANCE;

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();
  trigger_network_connections();


  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK_NO_THROW(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  import_initial_balances();

  BOOST_TEST_MESSAGE("Sending 1 million BTS to the next client in the list");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    uint32_t next_client_index = (i + 1) % client_processes.size();
    bts::blockchain::address destination_address = client_processes[next_client_index].rpc_client->getnewaddress("circle_test");
    bts::blockchain::asset destination_initial_balance = client_processes[next_client_index].rpc_client->getbalance(0);
    //bts::blockchain::asset source_initial_balance = client_processes[i].rpc_client->getbalance(0);
    const uint32_t amount_to_transfer = 1000000;
    client_processes[i].rpc_client->sendtoaddress(destination_address, amount_to_transfer);
    fc::time_point transfer_time = fc::time_point::now();
    for (;;)
    {
      fc::usleep(fc::milliseconds(500));
      if (client_processes[next_client_index].rpc_client->getbalance(0) == destination_initial_balance + amount_to_transfer)
      {
        BOOST_TEST_MESSAGE("Client " << next_client_index << " received 1MBTS from client " << i);
        break;
      }
      if (fc::time_point::now() > transfer_time + fc::seconds(35))
        BOOST_FAIL("Client did not report receiving the transfer within 35 seconds");
    }
  }
}

BOOST_AUTO_TEST_CASE(thousand_transactions_per_block)
{
  /* Create 1000 transfers from node 0 to all other nodes, */
  const uint32_t number_of_recipients = 10;
  const uint32_t number_of_transfers_to_each_recipient = 10;
  const uint32_t amount_of_each_transfer = 10;
  const uint32_t total_amount_to_transfer = amount_of_each_transfer * number_of_transfers_to_each_recipient * number_of_recipients;
  const uint64_t initial_balance_for_each_node =  std::max<uint64_t>(100000000, total_amount_to_transfer * 2); // allow for fees;


  client_processes.resize(number_of_recipients + 1);

  // we're only sending from node 0, but we need to set a balance for more than just one node
  // to allow the delegate vote to be spread out enough
  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].initial_balance = initial_balance_for_each_node;

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

  trigger_network_connections();

  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK_NO_THROW(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  import_initial_balances();

  std::vector<bts::blockchain::address> recieve_addresses;
  recieve_addresses.resize(number_of_recipients + 1);
  BOOST_TEST_MESSAGE("Generating receive addresses for each recieving node");
  for (unsigned i = 1; i < client_processes.size(); ++i)
    recieve_addresses[i] = client_processes[i].rpc_client->getnewaddress("test");

  BOOST_TEST_MESSAGE("Making 1000 transfers from node 0 to the rest of the nodes");
  unsigned total_transfer_count = 0;
  for (unsigned transfer_counter = 0; transfer_counter < number_of_transfers_to_each_recipient; ++transfer_counter)
  {
    for (unsigned i = 1; i < client_processes.size(); ++i)
    {
      client_processes[0].rpc_client->sendtoaddress(recieve_addresses[i], amount_of_each_transfer);
      // temporary workaround:
      ++total_transfer_count;
      if (total_transfer_count % 50 == 0)
      {
        BOOST_TEST_MESSAGE("sleeping for a block");
        fc::usleep(fc::seconds(30));
      }
    }
  }

  BOOST_TEST_MESSAGE("Done making transfers, waiting 30 seconds for a block to be formed");
  fc::usleep(fc::seconds(30));
  BOOST_TEST_MESSAGE("Collecting balances from recipients");

  uint64_t total_balances_recieved = 0;
  for (unsigned i = 1; i < client_processes.size(); ++i)
    total_balances_recieved += (client_processes[i].rpc_client->getbalance(0).amount - initial_balance_for_each_node);

  BOOST_TEST_MESSAGE("Recieved " << total_balances_recieved << " in total");
  BOOST_CHECK(total_balances_recieved == total_amount_to_transfer);

  // get the total amount of data transferred
  std::vector<std::pair<uint64_t, uint64_t> > rx_tx_bytes;
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    fc::variants peers_info = client_processes[i].rpc_client->getpeerinfo();
    uint64_t total_in = 0;
    uint64_t total_out = 0;
    for (const fc::variant& peer_info : peers_info)
    {
      fc::variant_object peer_info_object = peer_info.get_object();
      total_out += peer_info_object["bytessent"].as_uint64();
      total_in += peer_info_object["bytesrecv"].as_uint64();
    }
    rx_tx_bytes.push_back(std::make_pair(total_in, total_out));
  }
  BOOST_TEST_MESSAGE("Peer\tSent\tReceived");
  BOOST_TEST_MESSAGE("-----------------------------------------------------");
  for (unsigned i = 0; i < rx_tx_bytes.size(); ++i)
    BOOST_TEST_MESSAGE(i << "\t" << rx_tx_bytes[i].second << "\t" << rx_tx_bytes[i].first);
}

BOOST_AUTO_TEST_CASE(untracked_transactions)
{
  // This test will try to send a ton of transfers.
  // we'll launch 50 clients,
  // each with, say, 100M BTS
  // then send as many small transactions as it can,
  // transferring to the other nodes.
  // when a transfer fails, it will begin again with the next node

  client_processes.resize(50);

  const uint32_t expected_number_of_transfers = 1000 / client_processes.size();

  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].initial_balance = 100000000;

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();


  establish_rpc_connections();

  trigger_network_connections();

  int number_of_partitions = verify_network_connectivity(bts_xt_client_test_config::config_directory / "untracked_transactions");
  BOOST_REQUIRE_EQUAL(number_of_partitions, 1);

  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK_NO_THROW(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  import_initial_balances();

  std::vector<bts::blockchain::address> recieve_addresses;
  recieve_addresses.resize(client_processes.size());
  BOOST_TEST_MESSAGE("Generating receive addresses for each recieving node");
  for (unsigned i = 0; i < client_processes.size(); ++i)
    recieve_addresses[i] = client_processes[i].rpc_client->getnewaddress("test");

  //// initial setup is done

  uint32_t total_number_of_transactions = 0;
  fc::time_point test_start_time = fc::time_point::now();

  // let's try to generate 10 blocks
  uint32_t num_blocks = 10;
  for (uint32_t block_number = 0; block_number < num_blocks; ++block_number)
  {
    //BOOST_TEST_MESSAGE("Making as many 1000 transfers from node 0 to the rest of the nodes");
    uint32_t next_recipient = 1;
    uint32_t transactions_in_this_block = 0;
    for (unsigned process = 0; process < client_processes.size(); ++process)
    {
      for (unsigned transfer = 0; ; ++transfer)
      {
        try
        {
          client_processes[process].rpc_client->sendtoaddress(recieve_addresses[next_recipient], 10);
          next_recipient = (next_recipient + 1) % client_processes.size();
          if (next_recipient == process)
            next_recipient = (next_recipient + 1) % client_processes.size();
          ++transactions_in_this_block;
        }
        catch (const fc::exception&)
        {
          if (transfer >= expected_number_of_transfers)
            BOOST_TEST_MESSAGE("Sent " << transfer << " transfers from process " << process << " in round " << block_number);
          else
            BOOST_TEST_MESSAGE("Only able to send " << transfer << " transfers from process " << process << " in round " << block_number);
          BOOST_CHECK_GE(transfer, expected_number_of_transfers);
          break;
        }
      }
    }
    total_number_of_transactions += transactions_in_this_block;
    BOOST_TEST_MESSAGE("Done sending all transfers in round " << block_number << ", total of " << transactions_in_this_block << " transactions");
    BOOST_TEST_MESSAGE("Sleeping for 30 seconds to allow a block to be generated");
    fc::usleep(fc::seconds(30));
  }

  fc::time_point test_end_time = fc::time_point::now();

  // get the total amount of data transferred
  std::vector<std::pair<uint64_t, uint64_t> > rx_tx_bytes;
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    fc::variants peers_info = client_processes[i].rpc_client->getpeerinfo();
    uint64_t total_in = 0;
    uint64_t total_out = 0;
    for (const fc::variant& peer_info : peers_info)
    {
      fc::variant_object peer_info_object = peer_info.get_object();
      total_out += peer_info_object["bytessent"].as_uint64();
      total_in += peer_info_object["bytesrecv"].as_uint64();
    }
    rx_tx_bytes.push_back(std::make_pair(total_in, total_out));
  }

  BOOST_TEST_MESSAGE("Peer\tSent\tReceived");
  BOOST_TEST_MESSAGE("-----------------------------------------------------");
  for (unsigned i = 0; i < rx_tx_bytes.size(); ++i)
    BOOST_TEST_MESSAGE(i << "\t" << rx_tx_bytes[i].second << "\t" << rx_tx_bytes[i].first);
  uint32_t run_time_in_seconds = (uint32_t)(test_end_time - test_start_time).count() / fc::seconds(1).count();
  BOOST_TEST_MESSAGE("Test ran for " << run_time_in_seconds << " seconds");
  BOOST_TEST_MESSAGE("Total number of transactions: " << total_number_of_transactions << ", presumably in about " <<
                     (run_time_in_seconds / 30) << " blocks");
  BOOST_TEST_MESSAGE("That's about " << (total_number_of_transactions / run_time_in_seconds) << " transactions per second");
}

BOOST_AUTO_TEST_CASE(fifty_node_test)
{
  /* Note: on Windows, boost::process imposes a limit of 64 child processes,
           we should max out at 55 or less to give ourselves some wiggle room */
  client_processes.resize(50);

  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].initial_balance = INITIAL_BALANCE;

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

  trigger_network_connections();

  // wait a bit longer than the retry timeout so that if any nodes failed to connect the first
  // time (happens due to the race of starting a lot of clients at once), they will have time
  // to retry before we start checking to see how they did
  fc::usleep(fc::seconds(_peer_connection_retry_timeout * 5 / 2));

  verify_network_connectivity(bts_xt_client_test_config::config_directory / "fifty_node_test");

  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK_NO_THROW(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  import_initial_balances();

  bts::blockchain::address receive_address = client_processes[0].rpc_client->getnewaddress("test");
  bts::blockchain::transaction_id_type transaction_id = client_processes[0].rpc_client->sendtoaddress(receive_address, 50);
  fc::usleep(fc::seconds(10));  // give the transaction time to propagate across the network
  std::vector<bts::net::message_propagation_data> propagation_data;
  propagation_data.resize(client_processes.size());
  for (unsigned i = 0; i < client_processes.size(); ++i)
    propagation_data[i] = client_processes[i].rpc_client->_get_transaction_propagation_data(transaction_id);

  get_node_ids();

  create_propagation_graph(propagation_data, 0, bts_xt_client_test_config::config_directory / "fifty_node_test" / "transaction_propagation.dot");

  //auto min_iter = std::min_element(receive_times.begin(), receive_times.end());
  //auto max_iter = std::max_element(receive_times.begin(), receive_times.end());
  //std::cout << "node\tdelay\n";
  //std::cout << "---------------------------\n";
  //for (unsigned i = 0; i < client_processes.size(); ++i)
  //{
  //  std::cout << i << "\t" << (receive_times[i] - *min_iter).count() << "us";
  //  if (receive_times[i] == *min_iter)
  //    std::cout << "\t <-- min";
  //  else if (receive_times[i] == *max_iter)
  //    std::cout << "\t <-- max";
  //  std::cout << "\n";
  //}
}

BOOST_AUTO_TEST_SUITE_END()
