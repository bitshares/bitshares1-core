#define BOOST_TEST_MODULE BtsXtClientTests
#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

#include <sstream>
#include <iomanip>

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

struct bts_xt_client_test_config 
{
  static fc::path bts_client_exe;
  static fc::path config_directory;
  static uint16_t base_rpc_port;

  bts_xt_client_test_config() 
  {
    // parse command-line options
    boost::program_options::options_description option_config("Allowed options");
    option_config.add_options()("bts-client-exe", boost::program_options::value<std::string>(), "full path to the executable to test")
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

    std::cout << "Testing " << bts_client_exe.string() << "\n";
    std::cout << "Using config directory " << config_directory.string() << "\n";
    fc::create_directories(config_directory);

    boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
  }
};
fc::path bts_xt_client_test_config::bts_client_exe = "e:/Invictus/vs12_bt/programs/bts_xt/Debug/bts_xt_client.exe";
fc::path bts_xt_client_test_config::config_directory = fc::temp_directory_path() / "bts_xt_client_tests";
uint16_t bts_xt_client_test_config::base_rpc_port = 20100;

#define RPC_USERNAME "test"
#define RPC_PASSWORD "test"


BOOST_GLOBAL_FIXTURE(bts_xt_client_test_config);

fc::process_ptr launch_bts_client_process(uint32_t process_number)
{
  fc::process_ptr bts_client_process(std::make_shared<fc::process>());
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
    fc::path wallet_data_filename = wallet->get_wallet_file();
    wallet->create(wallet_data_filename, "", "testtest");
  }

  options.push_back("--data-dir");
  options.push_back(numbered_config_dir.string());

  options.push_back("--server");
  options.push_back("--rpcuser=" RPC_USERNAME);
  options.push_back("--rpcpassword=" RPC_PASSWORD);
  options.push_back("--rpcport");
  options.push_back(boost::lexical_cast<std::string>(bts_xt_client_test_config::base_rpc_port + process_number));

  bts_client_process->exec(bts_xt_client_test_config::bts_client_exe, options, numbered_config_dir);

  return bts_client_process;
}

struct test_client
{
  fc::process_ptr process;
  bts::rpc::rpc_client_ptr rpc_client;
};

BOOST_AUTO_TEST_CASE(transfer_test)
{
  std::vector<test_client> client_processes;

  const uint32_t test_process_count = 10;

  BOOST_TEST_MESSAGE("Launching " << test_process_count << " bts_xt_client processes");
  for (int i = 0; i < test_process_count; ++i)
  {
    test_client client_info;
    client_info.process = launch_bts_client_process(i);
    client_processes.push_back(client_info);
  }

  BOOST_TEST_MESSAGE("Establishing JSON-RPC connections to all processes");
  for (int i = 0; i < test_process_count; ++i)
  {
    client_processes[i].rpc_client = std::make_shared<bts::rpc::rpc_client>();
    client_processes[i].rpc_client->connect_to(fc::ip::endpoint(fc::ip::address("127.0.0.1"), bts_xt_client_test_config::base_rpc_port));
  }

  BOOST_TEST_MESSAGE("Logging in to JSON-RPC connections");
  for (int i = 0; i < test_process_count; ++i)
  {
    client_processes[i].rpc_client->login(RPC_USERNAME, RPC_PASSWORD);
  }

  BOOST_TEST_MESSAGE("Verifying all clients have zero balance");
  for (int i = 0; i < test_process_count; ++i)
  {
    bts::blockchain::asset balance = client_processes[i].rpc_client->getbalance(0);
    BOOST_CHECK(balance == bts::blockchain::asset());
  }


  //bts::blockchain::asset balance = rpc_client->getbalance(0);
  //std::cout << "got balance\n";
  //std::cout << "balance is " << (std::string)balance <<  "\n";

  try{
    BOOST_TEST_MESSAGE("sleeping");
    fc::usleep( fc::seconds(10) );
  } catch ( const fc::exception& e )
  {
    wlog( "${e}", ("e",e.to_detail_string() ) );
    throw;
  }
}
