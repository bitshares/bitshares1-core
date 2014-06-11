#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/cli/cli.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/http/connection.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>


using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::utilities;
using namespace bts::client;
using namespace bts::cli;

BOOST_AUTO_TEST_CASE( public_key_type_test )
{
   try { 
    auto k1 = fc::ecc::private_key::generate().get_public_key();
    auto k2 = fc::ecc::private_key::generate().get_public_key();
    auto k3 = fc::ecc::private_key::generate().get_public_key();

    FC_ASSERT( public_key_type( std::string( public_key_type(k1) ) ) == k1);
    FC_ASSERT( public_key_type( std::string( public_key_type(k2) ) ) == k2);
    FC_ASSERT( public_key_type( std::string( public_key_type(k3) ) ) == k3);
  } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( wif_format_test )
{
  try {
   auto priv_key = fc::variant( "0c28fca386c7a227600b2fe50b7cae11ec86d3bf1fbe471be89827e19d72aa1d" ).as<fc::ecc::private_key>();
   FC_ASSERT( bts::utilities::key_to_wif(priv_key) == "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" );
   FC_ASSERT( bts::utilities::wif_to_key( "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" ).valid() );
   wif_to_key( key_to_wif( fc::ecc::private_key::generate() ) );
  } FC_LOG_AND_RETHROW() 
}

template<typename T>
void produce_block( T my_client )
{
      auto head_num = my_client->get_chain()->get_head_block_num();
      auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
      bts::blockchain::advance_time( (int32_t)((next_block_production_time - bts::blockchain::now()).count()/1000000) );
      auto b = my_client->get_chain()->generate_block(next_block_production_time);
      my_client->get_wallet()->sign_block( b );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );
      FC_ASSERT( head_num+1 == my_client->get_chain()->get_head_block_num() );
}


/***
 *  This test case is designed to grow for ever and never shrink.  It is a complete scripted history
 *  of operations that should always work based upon a generated genesis condition.
 */
BOOST_AUTO_TEST_CASE( master_test )
{ try {
   bts::blockchain::start_simulated_time(fc::time_point::from_iso_string( "20200101T000000" ));

   vector<fc::ecc::private_key> delegate_private_keys;

   genesis_block_config config;
   config.precision         = BTS_BLOCKCHAIN_PRECISION;
   config.timestamp         = bts::blockchain::now();
   config.base_symbol       = BTS_BLOCKCHAIN_SYMBOL;
   config.base_name         = BTS_BLOCKCHAIN_NAME;
   config.base_description  = BTS_BLOCKCHAIN_DESCRIPTION;
   config.supply            = BTS_BLOCKCHAIN_INITIAL_SHARES;

   for( uint32_t i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
   {
      name_config delegate_account;
      delegate_account.name = "delegate" + fc::to_string(i);
      delegate_private_keys.push_back( fc::ecc::private_key::generate() );
      auto delegate_public_key = delegate_private_keys.back().get_public_key();
      delegate_account.owner = delegate_public_key;
      delegate_account.is_delegate = true;

      config.names.push_back(delegate_account);
      config.balances.push_back( std::make_pair( pts_address(fc::ecc::public_key_data(delegate_account.owner)), BTS_BLOCKCHAIN_INITIAL_SHARES/BTS_BLOCKCHAIN_NUM_DELEGATES) );
   }

   fc::temp_directory clienta_dir;
   fc::json::save_to_file( config, clienta_dir.path() / "genesis.json" );

   fc::temp_directory clientb_dir;
   fc::json::save_to_file( config, clientb_dir.path() / "genesis.json" );

   auto sim_network = std::make_shared<bts::net::simulated_network>();

   auto clienta = std::make_shared<client>(sim_network);
   clienta->open( clienta_dir.path(), clienta_dir.path() / "genesis.json" );
   clienta->configure_from_command_line( 0, nullptr );

   auto clientb = std::make_shared<client>(sim_network);
   clientb->open( clientb_dir.path(), clientb_dir.path() / "genesis.json" );
   clientb->configure_from_command_line( 0, nullptr );

   std::cerr << clientb->execute_command_line( "help" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_create walletb masterpassword" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_create walleta masterpassword" ) << "\n";

   int even = 0;
   for( auto key : delegate_private_keys )
   {
      if( even >= 30 )
      {
         if( (even++)%2 )
         {
            std::cerr << "client a key: "<< even<<" "<< clienta->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
         }
         else
         {
            std::cerr << "client b key: "<< even<<" "<< clientb->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
         }
         if( even >= 34 ) break;
      }
      else ++even;
   }

   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clienta->execute_command_line( "unlock 999999999 masterpassword" ) << "\n";
   std::cerr << clienta->execute_command_line( "scan 0 100" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clienta->execute_command_line( "close" ) << "\n";
   std::cerr << clienta->execute_command_line( "open walleta" ) << "\n";
   std::cerr << clienta->execute_command_line( "unlock 99999999 masterpassword" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance delegate31" ) << "\n";

   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance delegate30" ) << "\n";
   std::cerr << clientb->execute_command_line( "unlock 999999999 masterpassword" ) << "\n";

   std::cerr << clientb->execute_command_line( "wallet_account_create b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance b-account" ) << "\n";

   std::cerr << clientb->execute_command_line( "wallet_account_register b-account delegate30" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" );
   std::cerr << clientb->execute_command_line( "wallet_account_update_registration b-account delegate30 { \"ip\":\"localhost\"} true" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_transfer 33 XTS delegate31 b-account first-memo" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history delegate31" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   produce_block( clientb );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history delegate31" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_create_account c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 10 XTS b-account c-account to-me" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "blockchain_list_delegates" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_set_delegate_trust_level b-account 1" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 100000 XTS delegate32 c-account to-me" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 100000 XTS delegate30 c-account to-me" ) << "\n";
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_set_delegate_trust_level b-account 1" ) << "\n";
   // TODO: this should throw an exception from the wallet regarding delegate_vote_limit, but it produces
   // the transaction anyway.   
   // TODO: before fixing the wallet production side to include multiple outputs and spread the vote, 
   // the transaction history needs to show the transaction as an 'error' rather than 'pending' and
   // properly display the reason for the user.
   // TODO: provide a way to cancel transactions that are pending.
   std::cerr << clienta->execute_command_line( "wallet_transfer 100000 XTS delegate31 b-account to-b" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "blockchain_list_delegates" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_asset_create USD Dollar b-account \"paper bucks\" null 1000000000 1000" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "blockchain_list_registered_assets" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_asset_issue 1000 USD c-account \"iou\"" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 20 USD c-account delegate31 c-d31" ) << "\n";
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history delegate31" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "bid c-account 120 XTS 4.50 USD" ) << "\n";
   std::cerr << clientb->execute_command_line( "bid c-account 40 XTS 2.50 USD" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "blockchain_market_list_bids USD XTS" ) << "\n";
   // THis is an invalid order
   //std::cerr << clientb->execute_command_line( "bid c-account 210 USD 5.40 XTS" ) << "\n";
   //produce_block( clientb );
  // std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
 //  std::cerr << clientb->execute_command_line( "balance" ) << "\n";

   //std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   //std::cerr << clientb->execute_command_line( "wallet_account_balance" ) << "\n";

} FC_LOG_AND_RETHROW() }



class test_file
{
  fc::path _result_file;
  fc::path _expected_result_file;
public:
       test_file(fc::path result_file, fc::path expected_result_file)
         : _result_file(result_file), _expected_result_file(expected_result_file) {}

  bool compare_files(); //compare two files, return true if the files match
};

bool test_file::compare_files()
{
  //first check if files are equal size, if not, files are different
  size_t result_file_size = boost::filesystem::file_size(_result_file.string());
  size_t expected_result_file_size = boost::filesystem::file_size(_expected_result_file.string());
  bool file_sizes_equal = (result_file_size == expected_result_file_size);
  if (!file_sizes_equal)
    return false;

  //files may be equal since they have the same size, so check further
  std::ifstream lhs(_result_file.string().c_str());
  std::ifstream rhs(_expected_result_file.string().c_str());

  typedef std::istreambuf_iterator<char> istreambuf_iterator;
  return std::equal( istreambuf_iterator(lhs),  istreambuf_iterator(), istreambuf_iterator(rhs));
}

#ifdef WIN32
#include <Windows.h>
char** CommandLineToArgvA(const char* CmdLine, int* _argc)
{
  char** argv;
  char*  _argv;
  unsigned long   len;
  uint32_t   argc;
  CHAR   a;
  uint32_t   i, j;

  bool  in_QM;
  bool  in_TEXT;
  bool  in_SPACE;

  len = strlen(CmdLine);
  i = ((len+2)/2)*sizeof(void*) + sizeof(void*);

  argv = (char**)GlobalAlloc(GMEM_FIXED,
      i + (len+2)*sizeof(CHAR));

  _argv = (char*)(((PUCHAR)argv)+i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = false;
  in_TEXT = false;
  in_SPACE = true;
  i = 0;
  j = 0;

  while( a = CmdLine[i] ) {
      if(in_QM) {
          if(a == '\"') {
              in_QM = false;
          } else {
              _argv[j] = a;
              j++;
          }
      } else {
          switch(a) {
          case '\"':
              in_QM = true;
              in_TEXT = true;
              if(in_SPACE) {
                  argv[argc] = _argv+j;
                  argc++;
              }
              in_SPACE = false;
              break;
          case ' ':
          case '\t':
          case '\n':
          case '\r':
              if(in_TEXT) {
                  _argv[j] = '\0';
                  j++;
              }
              in_TEXT = false;
              in_SPACE = true;
              break;
          default:
              in_TEXT = true;
              if(in_SPACE) {
                  argv[argc] = _argv+j;
                  argc++;
              }
              _argv[j] = a;
              j++;
              in_SPACE = false;
              break;
          }
      }
      i++;
  }
  _argv[j] = '\0';
  argv[argc] = NULL;

  (*_argc) = argc;
  return argv;
}
#else //UNIX
  #include <wordexp.h>
#endif

using namespace boost;
program_options::variables_map parse_option_variables(int argc, char** argv);


BOOST_AUTO_TEST_CASE( regression_test )
{
  try 
  {
  //for each test directory in full test
  //  open testconfig file
  //  for each line in testconfig file
  //    add a verify_file object that knows the name of the input command file and the generated log file
  //    start a process with that command line
  //  wait for all processes to shutdown
  //  for each verify_file object,
  //    compare generated log files in datadirs to golden reference file (i.e. input command files)

    vector<test_file> tests;
    std::ifstream test_config_file("test.config");
    string line;
    while (std::getline(test_config_file,line))
    {
      //parse line into argc/argv format for boost program_options
      int argc = 0; 
      char** argv = nullptr;
    #ifndef WIN32 // then UNIX 
      //use wordexp
      wordexp_t wordexp_result;
      wordexp(line.c_str(), &wordexp_result, 0);
      auto option_variables = parse_option_variables(wordexp_result.we_wordc, wordexp_result.we_wordv);
      argv = wordexp_result.we_wordv;
      argc = wordexp_result.we_wordc;
    #else
      //use ExpandEnvironmentStrings and CommandLineToArgvW
      argv = CommandLineToArgvA(line.c_str(),&argc);
      auto option_variables = parse_option_variables(argc, argv);
    #endif
      //extract input command file from cmdline options
      fc::path input_file( option_variables["input-log"].as<std::string>() ); 
      std::ifstream input_stream(input_file.string());
      fc::path expected_result_file = input_file;

      //run client with cmdline options
      bts::client::client_ptr client = std::make_shared<bts::client::client>();
      client->configure_from_command_line(argc,argv);
    #ifndef WIN32 // then UNIX 
      wordfree(&wordexp_result);
    #else
      GlobalFree(argv);
    #endif

      //add a test that compares input command file to client's log file
      fc::path result_file = client->get_data_dir() / "console.log";
      tests.push_back( test_file(result_file,expected_result_file) );
    } //end while not eof
    for (test_file current_test : tests)
    {
      //current_test.compare_files();
      FC_ASSERT(current_test.compare_files(), "Results mismatch with golden reference log");
    }
  } 
  catch ( const fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string() ) );
  }
}
