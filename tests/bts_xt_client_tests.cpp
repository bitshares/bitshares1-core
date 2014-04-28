#define BOOST_TEST_MODULE BtsXtClientTests
#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>
#include <boost/algorithm/string/join.hpp>

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

    std::cout << "Testing " << bts_client_exe.string() << "\n";
    std::cout << "Using config directory " << config_directory.string() << "\n";
    fc::create_directories(config_directory);

    boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
  }
};
fc::path bts_xt_client_test_config::bts_client_exe = "e:/invictus/vs12_bt/programs/bts_xt/Debug/bts_xt_client.exe";
fc::path bts_xt_client_test_config::bts_server_exe = "e:/invictus/vs12_bt/programs/bts_xt/Debug/bts_xt_server.exe";
fc::path bts_xt_client_test_config::config_directory = fc::temp_directory_path() / "bts_xt_client_tests";
uint16_t bts_xt_client_test_config::base_rpc_port = 20100;
uint16_t bts_xt_client_test_config::base_p2p_port = 21100;
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
  bts::rpc::rpc_client_ptr rpc_client;

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
    options.push_back("--connect-to");
    options.push_back(std::string("127.0.0.1:") + boost::lexical_cast<std::string>(bts_xt_client_test_config::base_p2p_port));
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

  //const uint32_t test_process_count = 10;
  
  void create_trustee_and_genesis_block();
  void launch_server();
  void launch_clients();
  void establish_rpc_connections();
  void import_initial_balances();
};

void bts_client_launcher_fixture::create_trustee_and_genesis_block()
{
  // generate a genesis block giving 100bts to each account
  BOOST_TEST_MESSAGE("Generating keys for " << client_processes.size() << " clients");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].private_key = fc::ecc::private_key::generate();
    genesis_block.balances.push_back(std::make_pair(bts::blockchain::pts_address(client_processes[i].private_key.get_public_key()), 
                                                    client_processes[i].initial_balance));
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
    fc::optional<bts::net::genesis_block_config> optional_genesis_block;
    if (i == 0 && !bts_xt_client_test_config::test_client_server)
      optional_genesis_block = genesis_block;
    client_processes[i].launch(i, trustee_key, i == 0, 
                               optional_genesis_block);
  }
}

void bts_client_launcher_fixture::establish_rpc_connections()
{
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

void bts_client_launcher_fixture::import_initial_balances()
{
  BOOST_TEST_MESSAGE("Importing initial keys and verifying initial balances");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->import_private_key(client_processes[i].private_key.get_secret(), "blah");
    client_processes[i].rpc_client->rescan(0);
    BOOST_CHECK(client_processes[i].rpc_client->getbalance(0) == client_processes[i].initial_balance);
  }
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

  client_processes[0].initial_balance = 1000000; // not important, we just need a nonzero balance to avoid crashing

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

  BOOST_TEST_MESSAGE("Testing a wallet operation without logging in");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    BOOST_CHECK_THROW(client_processes[i].rpc_client->getbalance(0), fc::exception)
  }

  BOOST_TEST_MESSAGE("Testing a wallet operation without logging in");
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
    BOOST_CHECK(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  BOOST_TEST_MESSAGE("Testing receive address generation");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    std::unordered_map<bts::blockchain::address, std::string> initial_addresses = client_processes[i].rpc_client->list_receive_addresses();
    BOOST_CHECK(initial_addresses.empty());
    std::string accountName("address_test_account");
    bts::blockchain::address new_address = client_processes[i].rpc_client->getnewaddress(accountName);
    BOOST_CHECK(initial_addresses.find(new_address) == initial_addresses.end());
    std::unordered_map<bts::blockchain::address, std::string> final_addresses = client_processes[i].rpc_client->list_receive_addresses();
    BOOST_CHECK(final_addresses.size() == initial_addresses.size() + 1);
    for (auto value : initial_addresses)
    {
      BOOST_CHECK(final_addresses.find(value.first) != final_addresses.end());
    }
    BOOST_CHECK(final_addresses.find(new_address) != final_addresses.end());
    BOOST_CHECK(final_addresses[new_address] == accountName);
  }
}

BOOST_AUTO_TEST_CASE(unlocking_test)
{
  client_processes.resize(1);

  client_processes[0].initial_balance = 1000000; // not important, we just need a nonzero balance to avoid crashing

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

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

  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
  }

  import_initial_balances();

  BOOST_TEST_MESSAGE("Sending 1 million BTS to the next client in the list");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    uint32_t next_client_index = (i + 1) % client_processes.size();    
    bts::blockchain::address destination_address = client_processes[next_client_index].rpc_client->getnewaddress("circle_test");
    bts::blockchain::asset destination_initial_balance = client_processes[next_client_index].rpc_client->getbalance(0);
    bts::blockchain::asset source_initial_balance = client_processes[i].rpc_client->getbalance(0);
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
  const uint32_t number_of_transfers_to_each_recipient = 100;
  const uint32_t amount_of_each_transfer = 1000;
  const uint32_t total_amount_to_transfer = amount_of_each_transfer * number_of_transfers_to_each_recipient * number_of_recipients;

  client_processes.resize(number_of_recipients + 1);
  client_processes[0].initial_balance = total_amount_to_transfer * 2; // allow for fees

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

  BOOST_TEST_MESSAGE("Opening and unlocking wallets");
  for (unsigned i = 0; i < client_processes.size(); ++i)
  {
    client_processes[i].rpc_client->open_wallet();
    BOOST_CHECK(client_processes[i].rpc_client->walletpassphrase(WALLET_PASPHRASE, fc::microseconds::maximum()));
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
      if (total_transfer_count % 100 == 0)
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
    total_balances_recieved += client_processes[i].rpc_client->getbalance(0).amount;

  BOOST_TEST_MESSAGE("Recieved " << total_balances_recieved << " in total");
  BOOST_CHECK(total_balances_recieved == total_amount_to_transfer);
}

BOOST_AUTO_TEST_CASE(one_hundred_node_test)
{
  return;
  client_processes.resize(100);

  for (unsigned i = 0; i < client_processes.size(); ++i)
    client_processes[i].initial_balance = INITIAL_BALANCE;

  create_trustee_and_genesis_block();

  if (bts_xt_client_test_config::test_client_server)
    launch_server();

  launch_clients();

  establish_rpc_connections();

  for (unsigned i = 0; i < client_processes.size(); ++i)
    BOOST_CHECK(client_processes[i].rpc_client->getconnectioncount() >= 3);
}

BOOST_AUTO_TEST_SUITE_END()