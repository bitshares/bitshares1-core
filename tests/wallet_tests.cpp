#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
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
#include <iostream>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/http/connection.hpp>

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::client;
using namespace bts::cli;

/*
  "5KYn77SMFximbA7gWoxZxs8VpxnzdnxJyRcA2hv7EziWdNk7cfX",
  "5KK44Qy6MUcBVHzPo3mHuKzzj5fzmRUrKbnnQE2gCYY4yDnsbyJ",
  "5KkUdWJ1VQAssZ3YY6chkG9uaWmhvPhkKsjRtTxV9KbqXfXy8dx",
  "5HwCeJNdh5PCimAZZMwTW4HfbziPbFE4wA5JadXaSRAi4zp3mgA",
  "5KkTHkXCcik9cDKoMo4BPxrojzjmFBwgEMEuCusoV5fdXhwwf19",
  "5JBUAW2EWd2TFfk2tVQmi86dGPejGedN8tNsHDGUJaVyW8bqjaK",
  "5KV5f8Eohi7WmuxWnP4uJzh7oX9zDEYpTbHBx5jFokSEZoUWpbB"
  */
const char* test_wif_keys = R"([
  "5KU3Sj7LtRi6A4tFZ2XapADeqizZ3SA7fNxc8KByjXKxi21M42x",
  "5HreFY4QaddTbb6AUKph52fQ3cbmEUpEePRy525ZTMLSdckqYve",
  "5J8PQtu1HY4yb8o8dsDL3CxmxV8Tro2AK9LWaQj2jKeu227tEm2",
  "5KE5bjhx5RJT7xPFFPMkvj9YEdpf47qGbhY23QjEmkkEaoC7PCa",
  "5KJXiBjJ4APATGPoLqsyssLKjKBfMSNEa3oGeQNaZnrqpe61iwz",
  "5JzMqDwLVHMBbJqbqS2BfS1EEBQWC5Sb3Rkievn8xvMMj6SEt3J",
  "5K6kX9TK6b4wwh5GdRuyguLZEZcDkSBqer2RBaiorebjzLQFpYo",
  "5KJzxHADuPLLftGjkc6zF6r1oRsQ36PgSrowjmGfoQRbbLDjx2z",
  "5J5sUufthtA6xUv9JgmSfbvZWpKbcDR94oGUVqK35muLDsvuAja",
  "5JPtJZvRWcrKBvV2aA6TwvUGxKjTdNMvmK6ENHSuLUtAQ7AWVk5",
  "5JqwCevx4dYTYBNX1yewoBZoJTbTXMpxs2C5a1Z3Gpo3q1x3yRN",
  "5KNuwnpLeK4xGRAq3Ejkm2hu1H9gDmzAYSzWDnYPAdrsZLS9Mvu",
  "5JdsDeUN5gxbtUgiWc7AynPyWQTiaPaMQ3zzYEgs6bZchpjnwJL",
  "5JrbaumXBXPcTq5fSdzcc9x7T1LcbEqQDqUxJeQgQDkit7GonVS",
  "5JtxciS4Q8YxGRe3uBwznTuy9fU4qdSUNq15GA6WXiXotUDzEyg",
  "5J7dwmyMKWi6fZ5zasHQqTvnKb283Dpg3gpwCJLPy9uK8MfUTJw",
  "5KExAZQaqpeWZj54BawxhAvbAqwF82rwYjyfvLmwEYAsMoKnoJ3",
  "5KWi5CTq1g4zHa3gzaieZDmikxGd3EZe4WsUw4yDvSqheWoPbBy",
  "5JTyyJTuC9uUNZNprYK9rar7q53Lw9FgEn4knq6n8d1xCA9Wszx",
  "5JJahsHQUYbAm3h2gPx5cL4VxdXuYJQgF9D2sTikGEqbG4EX8kd",
  "5KDPBK24FH5KnwQHiNKwRz3gokj9HFbcbq3Z6GwZws9EXW7VQaA",
  "5JHy4a38rsJD9j3E8k3BHp6ft65QMYfXySC4yDgWB3VCvjSP4Gw",
  "5Hz164itq48DMsVQ1W7iCnBdSEzBJjZoPCKm8JjJPFC9mvFuLZF",
  "5KCeRHGXXLHwkMApxkoTvuM23RmmFCqPjvCNc8Dg8BkjTeKWBph",
  "5KP8LxCczF54AQU1RjVVUm7rXXCnhDPkPCzzhAtzULEnJixir4j",
  "5JwYKFbguekptzgfg6xetA2Dg3ruZAx9oPR3bvZTMLZ3L1TmxF1",
  "5KV1Pmh6o5hRpCYZjSf1zVhGM4eihGc3yBJMSWnK8B66yfPJhSD",
  "5KdeCK6PcFuZqh9XBPBN2tYG2MsgXpYXG3vzPhLK1VA8ye6Ptwf",
  "5HpfB8gzfLXps46uZAKXrrN5XdrBMUSvAgwsxpecmG35HDQQvBf",
  "5Hwj2ty8b7V87EqJhTHLYEFiLXu67UC2fGNTwCjMqcDTh9k5iZ3",
  "5HrXYt7PHuxwiGq5NY49Um5uXS1azQjdebCfLBFTfeeCXSRA5bs",
  "5Kgv87goW599UgHxLYuVTVJMnajgh6iJLjzuh54g2kiDmP7jX4e",
  "5JGeHYXq9BRmvPuEpEoSJmGxpBo3MX6pxb9Fty5qwF7wrGqqf6x",
  "5JZXgPtPu3X7L2rBPk966FCcFJ7kor6ozbH2Akbty9fXk8y7Tp2",
  "5Jnbv8bKTPzesPK2qBKKEgMtoGAWg78z9zVB7KAKgskrpD9Cvyk",
  "5JYSUVEVSh8gH3AYzbfkg4J14P8ZzTHYwzcpX1b9qyZdESR2LEP",
  "5J6zzXxh5Q1MRJuVoEHvtDSkckBRqECzQM9heBn45aAcr9ityEi",
  "5JwGf7nET2iXvgDxRTyKPdayZ6ygBbbEm42iAmFimwoY5iSqLEn",
  "5KfCWVL9iQPzzBvCFi6YNdKScqsMkW5MKoJVXePDxeFh8xsbqLK",
  "5KHA9x5YEBpT3z2VmX3nj1Xd5DXaFqFNKpeHauJCqCb1wmQjLQu",
  "5JgNYmQ66SFWqmJfYJ1jGgjemwkwPiwYAvyUzF1LBZ1iS7VBwD9",
  "5Ju9sj7ZV4tAfnWa5mQrVB8LVYfyyYtGqLhZdqXANH26WbeouAk",
  "5KD11E7kLNUcG4cg3o7KBUcecRypxdZdHMcZ3W1yhKjKKpqFn8Z",
  "5Km1pQFzeTkATSc1vbg4ZTCBYKNuQycADqTjFDKi6hEKVihEayb",
  "5JGsBi6e2oSQJgot4JWabXTxTaoHRxk2AYkUFSaPVgasSFFzf3E",
  "5Jn3An5DcCcHb6dTLEFVBkJA6amk92gNzvShCmZQbpvBp3PCr76",
  "5KWnZLdoLKPuiyVdGFjCDaxY4P6xnHymYnTrtF6H69bqSSewEpJ",
  "5JVrBxpMWVQUKV3WmqFxb6YunFBnDCav7Wnw8BuhzDL5gu9STbS",
  "5Jsy1haER1y77qH9LqwQ151NxdaobAki7aB9AKHPbtJYvC34n5U",
  "5HwejMdWjwnFX9USZwNHH5jfSHNqSPw5MmKr5i7AUCFBTLVVqCw",
  "5JfuDWFRscdtqR9WLSdK6LUXRZ3rxUoHKGvDxSjrRVVaN1aMbHn",
  "5K4ziASEV1jS3Fq5qh13GprKhg22LbNqpvpseQ285W1i2XC3kwr",
  "5JaUEtp9UfZQaWrX2RXhHxKAkZjzSoNbwgwSAHTxHKfNMRRARB8",
  "5KadZxFfCCCRyWjPinevvooNwscEyzxQum1s6bzoCJkyL5RXH7W",
  "5JQJCwLvutsH6og2hvaoAUCwcRc5t9cVxaixwzZyoAq1QH9RQfA",
  "5Jy9BVSbqJ97MWR8aASZA61efdBKqTwDZ1MwPyyAPA8xceuxQWq",
  "5KVBpVVtWUsMQc2qQ825NLGz8ekmMDVFodcDxcZdbr4nf5n6JZH",
  "5K6r972NZdZcfrijhqP8JKteYwtXERW1x1ttxJ6eHqoCtAewrKw",
  "5JxiMgVUZowuQXsRZcTvqReGeDvbqsxk5dtyShZhzxC95bHbCML",
  "5JYC5MqCqqAKry4kZfm4SWE7tDtiiBGoHUKotepsvo2kjnTrjcS",
  "5JRFhJ7pGVUJ536bqGbQT8Hoa2k1eE9L43hyENg8FcV4nY6BLTH",
  "5KgYU6mbT5sT5EaPZVskaDPtR7GinsZZjdD3jA8UgyxQ7j4fQij",
  "5J9yrpW7knihTffrw3mDKPQ88vvqsLJdmytrXNvgRs9d5Sd2zs4",
  "5JL94PhyHhhYxau5dNeWNveDCdvgUJFi86aLPcDaCT42bUQVpBK",
  "5JeFhDeTnkUonnnBn2hWFAcPytxDNVN1oGNNSRTSdNoWk6QG2WA",
  "5JxVY4m4p8wgV3MDwMPLkp8mGEsZMyqULZ9E7Qymx6o1jjx7GH9",
  "5KQc958faU3ySTicJV7LojvaZ1Y8UU8ujQ6LJHixLProsoghpne",
  "5JUN4YAZy1S39HxqGAp1Q3h3BZgLUzo9qZAnZGxuuEioTCad8rM",
  "5KkZvkFQMqgHBdnuzXKYNLQiaYfGgLDNZSZpYyQoJLiFmRaA4dT",
  "5JxEbtPEBDEtDEibbarKFPwcQgsxc96mxmj9wGuYA8p5UnU4HXd",
  "5JqK5zY1ugvqpvtT5LwwB8Kve8fZXwM2ktyGU1yU9N6ita2jwg8",
  "5JmAFR4cfEuexZ22T4GRa9KfjLJT2JGoKjT49Cag8GDYziLVpbr",
  "5K8kfi4jtzUaMZ6ebxjSyQLTZPEwhsoBwP79KC32JRfkRsJjCSu",
  "5JgvdQMgQZogaL3qLu4tRWr93YnYTXqgr1KFKW9vfWfjVs3LHrb",
  "5KgHkSE6gatznsZ24C1fS5ViTEkGa19vyYCHMhbjmgnLDYZdkJE",
  "5KhhahvxRDBYPX9xQWPRqzfhiKmyxxdEeodG4pG1WMzV3MeiYao",
  "5JYVxi97dYtm6RQDm4oP19gPfqe36dGyWHaMWB5k3KPBaEsb6zJ",
  "5HpPRmPUfZnLoHPRTfN5JDtdg6d5FN7LtSy1tanSx7gb9eDQNsL",
  "5Hvnzv3Htgo3MNM8GWjp8yjmvXoZSEV3n9WNqtYUHaiVA67sgmx",
  "5JkAwXxT8XnECAUec7t6xPfjw7PTmmtA8FPSFqwMXNdTW6SpGCM",
  "5KLeJzBYhabKt1LLFXDvG79Wt7y6AKnMbhN1w63BsuspD6uj1P1",
  "5KKsaK8xs1n6T1F3HN6cjME2PJnncJ8MEzw6JwJAkjMmdBLfhbk",
  "5Jn8WutwvNfnFQRrdwErpkVFK9MgJWP99T31E2q9PvkVFan5uMZ",
  "5KhJxJnuTWL6qux2SutCjxw3qourPV6MdcppS4FE7bGuS1rCWVY",
  "5JnT3fBz8LxZGgoWZQE5Uw788tLiND414T5HPP2MDXehrncn657",
  "5JFzi3cjJiYmpii87d91yWHDEiXtKrTwNyiQ5G6uKPzdpLvDovm",
  "5JBbz6T1a1Ta2FCJk9gTXwNEkbZ8Z4hhJNmxxSJtaxDmoC1XpAY",
  "5K3XPKfxDFTSZGkKew1gSm1J6vp6YsaQteUqKQZEvre2MdFB59b",
  "5JNTkmH4exbEybEi7Y9X853oa1nWKTDvKgDLKJ94z6FsfrkF5Qx",
  "5JbVnW1eANRYefx4vWaEwKdnkFdnVyqt8pM5gcNcrypGXv14Ec1",
  "5Km1XYkQYiRbVcqBybUMaMVQC5ytif8fUyyF4k4QeWJbxraqdqU",
  "5KZqXPfCuAugJtsWFLkLeBg4hQuwjSGS4YJrwmhaxXN6v2CTTKf",
  "5K73XPNrzwBo9Z6tfS1dKTa4Z4ENoVi6WbtpiuEP41KmJtvn173",
  "5JCDLGUWqFgk2FFTxCsjbrFpPrZBJXrLjrXbYZdTPwMEhxiePkn",
  "5JKJtSc6D2xP32oF3oFd8gPuGLXUktAWjkUL6kVn4y2g38Nb3ha",
  "5KT8eCBqgdhFvgndjvZvUh2xzw1QNbHapPC83DKqWPuk53ABs7x",
  "5KLwx1TpBJen1D5FvDgRvBwGDTttXCkCQxkK7YdPdg18nEmLBce"
])";

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

   auto priv_key = fc::variant( "0c28fca386c7a227600b2fe50b7cae11ec86d3bf1fbe471be89827e19d72aa1d" ).as<fc::ecc::private_key>();
   FC_ASSERT( bts::utilities::key_to_wif(priv_key) == "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" );
   FC_ASSERT( bts::utilities::wif_to_key( "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" ).valid() );
   /*
   auto secret = priv_key.get_secret();
   auto secret_hash = fc::sha256::hash( (char*)&secret, sizeof(secret) );
   auto secret_hash_str = "80" + std::string(secret_hash);
   FC_ASSERT( secret_hash_str == "8147786c4d15106333bf278d71dadaf1079ef2d2440a4dde37d747ded5403592" );
   auto secret_hash2 = fc::sha256::hash( secret_hash );
   FC_ASSERT( std::string(secret_hash2) == "507a5b8dfed0fc6fe8801743720cedec06aa5c6fca72b07c49964492fb98a714" );
   */

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

#include <fstream>

string extract_commands_from_log_stream(std::istream& log_stream)
{
  string command_list;
  string line;
  while (std::getline(log_stream,line))
  {
    //if line begins with a prompt, add to input buffer
    size_t prompt_position = line.find(CLI_PROMPT_SUFFIX);
    if (prompt_position != string::npos )
    { 
      size_t command_start_position = prompt_position + strlen(CLI_PROMPT_SUFFIX);
      command_list += line.substr(command_start_position);
      command_list += "\n";
    }
  }
  return command_list;
}

string extract_commands_from_log_file(fc::path test_file)
{
  std::ifstream test_input(test_file.string());
  return extract_commands_from_log_stream(test_input);
}

BOOST_AUTO_TEST_CASE( client_tests )
{
   /*
   if( !fc::exists( "xt_genesis.json" ) )
   {
      auto con = std::make_shared<fc::http::connection>();
      con->connect_to( fc::ip::endpoint::from_string( "107.170.30.182:80" ) );
      wlog( "fetching genesis block, this could take a few seconds" );
      auto response = con->request( "GET", "http://bitshares.org/snapshots/xt_genesis.json" );
      FC_ASSERT( response.body.size() );
      auto check = fc::variant("154fb7a037f139b235a79ef82df474c6ab06456fece9ddbda18787cb1d12b556").as<fc::sha256>();
      FC_ASSERT( fc::sha256::hash( response.body.data(), response.body.size() ) == check );
      std::ofstream out( "xt_genesis.json", std::ios::binary );
      out.write( response.body.data(), response.body.size() );
   }
   */

   try {
      std::string password = "123456789";
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;

      auto test_keys = fc::json::from_string( test_wif_keys ).as<vector<string> >();

      auto network = std::make_shared<bts::net::simulated_network>();

      auto my_client = std::make_shared<client>(network);
      my_client->open( my_dir.path() );


      auto your_client = std::make_shared<client>(network);
      your_client->open( your_dir.path() );

      /* DLN: Some example test code, just left here for reference, will remove soon
      std::ofstream console_log("notestein_wallet_test.log");
      //std::stringstream my_input("wallet_list\n");      
      auto my_cli = new bts::cli::cli( my_client, my_input, console_log);      
      my_cli->set_input_log_stream(console_log);
      my_cli->process_commands();
      my_cli->wait();
      */
      
      /* DLN: Example that extracts commands from an existing log file
              and sends them to client for re-execution. Results are
              written to std::cerr in this example.
      string input_buffer = extract_commands_from_log_file("console.log");
      std::stringstream my_input(input_buffer);

      auto my_cli = new bts::cli::cli( my_client, my_input, std::cerr);  
      my_cli->set_input_log_stream(std::cerr);
      my_cli->process_commands();
      my_cli->wait();
      */
          
      auto my_cli = new bts::cli::cli( my_client, &std::cin, &std::cerr);  
      auto your_cli = new bts::cli::cli( your_client, &std::cin, &std::cerr);      

      my_cli->execute_command_line( "help");
      my_cli->execute_command_line( "blockchain_list_delegates" );
      my_cli->execute_command_line( "wallet_create my_wallet "+password );
      my_cli->execute_command_line( "wallet_unlock 999999999999 "+password );
      your_cli->execute_command_line( "wallet_create your_wallet "+password );
      your_cli->execute_command_line( "wallet_unlock 999999999999 "+password );

      /*
      my_client->wallet_create( "my_wallet", password );
      my_client->wallet_unlock( fc::seconds(999999999), password );
      your_client->wallet_create( "your_wallet", password );
      your_client->wallet_unlock( fc::seconds(999999999), password );
      */

      my_cli->execute_command_line( "wallet_import_private_key " + test_keys[30] + "\"\" true" );
      my_cli->execute_command_line( "wallet_import_private_key 5KJ51szQb1CDcU9AkzKSDAkiYbL5V6CnVA5SWYG1NfsMr7B3HDS delegate30 true" );
      my_cli->execute_command_line( "balance" );
      my_cli->execute_command_line( "wallet_close" );
      my_cli->execute_command_line( "wallet_open my_wallet" );
      my_cli->execute_command_line( "wallet_unlock 999999999999 "+password );
      my_cli->execute_command_line( "wallet_account_create account1" );
      my_cli->execute_command_line( "wallet_list_receive_accounts" );
      my_cli->execute_command_line( "wallet_account_register account1 delegate30 null true" );
      produce_block( my_client );
      my_cli->execute_command_line( "wallet_list_receive_accounts" );

      FC_ASSERT( my_client->get_info()["blockchain_head_block_num"].as_int64() == your_client->get_info()["blockchain_head_block_num"].as_int64() );
      FC_ASSERT( your_client->blockchain_list_registered_accounts("account1",1)[0].name == "account1" );

      public_key_type your_account_key = your_client->wallet_account_create( "youraccount" );
      my_client->wallet_add_contact_account( "youraccount", your_account_key );

      public_key_type other_account_key = your_client->wallet_account_create( "otheraccount" );
      my_client->wallet_add_contact_account( "otheraccount", other_account_key );

      wlog( "your cli" );
      your_cli->execute_command_line( "wallet_account_transaction_history" );
      std::cerr<<"\n";
      your_cli->execute_command_line( "balance" );

      for( uint32_t i = 0; i < 10; ++i )
      {
         auto start = fc::time_point::now();
         for( uint32_t x = 0; x < 1; ++x )
         {
            my_client->wallet_transfer( (30+i*100+x)/10.0, "XTS", "delegate30", "youraccount", "memo-"+fc::to_string(i) );
            my_client->wallet_transfer( (20+i*100+x)/10.0, "XTS", "delegate30", "otheraccount", "memo-"+fc::to_string(i) );
         }
         produce_block( my_client );
         auto end = fc::time_point::now();
         elog( "block production time: ${t}", ("t", (end-start).to_seconds() ) );
      }
      /*
      for( uint32_t i = 0; i < 1000; ++i )
      {
         produce_block( my_client );
      }
      */
      my_cli->execute_command_line( "wallet_account_transaction_history" );

      auto trx = your_client->wallet_account_register( "youraccount", "youraccount", variant(), true );
      produce_block( my_client );
      trx = your_client->wallet_account_register( "otheraccount", "otheraccount", variant(), false );
      produce_block( my_client );
      trx = your_client->wallet_account_update_registration( "otheraccount", "otheraccount", variant(), true );
      produce_block( my_client );

      //auto result = my_client->wallet_list_unspent_balances();
//      my_cli->execute_command_line( "wallet_list_unspent_balances" );
      wlog( "my cli" );
      my_cli->execute_command_line( "wallet_account_transaction_history youraccount" );
      my_cli->execute_command_line( "wallet_account_transaction_history otheraccount" );
      my_cli->execute_command_line( "wallet_account_transaction_history \"delegate30\"" );
      std::cerr<<"\n";
      my_cli->execute_command_line( "balance" );
      std::cerr<<"\n";
      std::cerr<<"\n";
      wlog( "your cli" );
      your_cli->execute_command_line( "wallet_account_transaction_history" );
      std::cerr<<"\n";
      your_cli->execute_command_line( "balance" );
      wlog("my");
      my_cli->execute_command_line( "wallet_list_contact_accounts" );
      my_cli->execute_command_line( "wallet_list_receive_accounts" );
      wlog("your");
      your_cli->execute_command_line( "wallet_list_contact_accounts" );
      your_cli->execute_command_line( "wallet_list_receive_accounts" );

      your_cli->execute_command_line( "wallet_asset_create USD BitUSD youraccount \"description\" null 1000000000000000  1000" );
      produce_block( my_client );
      your_cli->execute_command_line( "wallet_account_transaction_history youraccount" );
      your_cli->execute_command_line( "blockchain_list_registered_assets" );
      your_cli->execute_command_line( "wallet_asset_issue 50000000 USD otheraccount \"some memo\"" );
      produce_block( my_client );
      your_cli->execute_command_line( "wallet_account_transaction_history youraccount" );
      your_cli->execute_command_line( "blockchain_list_registered_assets" );
      your_cli->execute_command_line( "balance" );
   //   return;
      your_cli->execute_command_line( "transfer 2000000 USD otheraccount youraccount \"payday\"" );
      produce_block( my_client );
      your_cli->execute_command_line( "balance" );
      //your_cli->execute_command_line( "unlock 99999999999999999999" );
      my_cli->execute_command_line( "wallet_submit_proposal delegate30 \"test proposal\" \"test body\" \"notice\" null" );
      produce_block( my_client );
      my_cli->execute_command_line( "wallet_account_transaction_history" );
      my_cli->execute_command_line( "blockchain_list_delegates 0 3" );
      my_cli->execute_command_line( "wallet_withdraw_delegate_pay delegate30 delegate30 100 \"del payday\"" );
      produce_block( my_client );
      my_cli->execute_command_line( "wallet_account_transaction_history" );
      my_cli->execute_command_line( "blockchain_list_proposals" );
      my_cli->execute_command_line( "blockchain_get_proposal_votes 1" );
      my_cli->execute_command_line( "wallet_vote_proposal delegate30 1 yes \"why not\"" );
      produce_block( my_client );
      my_cli->execute_command_line( "wallet_account_transaction_history" );
      my_cli->execute_command_line( "blockchain_list_proposals" );
      my_cli->execute_command_line( "blockchain_get_proposal_votes 1" );
      my_cli->execute_command_line( "info" );
      your_cli->execute_command_line( "balance" );
      // this errors as expected because youraccount is not a delegate
      // your_cli->execute_command_line( "wallet_submit_proposal youraccount \"test proposal\" \"test body\" \"notice\" null" );


      
      //ilog( "unspent:\n ${r}", ("r", fc::json::to_pretty_string(result)) );

//      ilog( "my_client ${info}", ("info", fc::json::to_pretty_string(my_client->get_info()) ));
//      ilog( "your_client ${info}", ("info", fc::json::to_pretty_string(your_client->get_info()) ));
//      ilog( "registered_names: ${info}", 
                                                                                                                                                                                                                                                                                                                                                                                                                                   

   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
} // client_tests


BOOST_AUTO_TEST_CASE( delegate_proposals )
{
   try {
      return;
      std::string password = "123456789";
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;

      auto test_keys = fc::json::from_string( test_wif_keys ).as<vector<string> >();

      auto network = std::make_shared<bts::net::simulated_network>();

      auto my_client = std::make_shared<client>(network);
      my_client->open( my_dir.path() );

      auto your_client = std::make_shared<client>(network);
      your_client->open( your_dir.path() );

      auto my_cli = new bts::cli::cli( my_client, &std::cin, &std::cerr);  
      auto your_cli = new bts::cli::cli( your_client, &std::cin, &std::cerr);      

      my_client->wallet_create( "my_wallet", password );
      my_client->wallet_unlock( fc::seconds(999999999), password );
      your_client->wallet_create( "your_wallet", password );
      your_client->wallet_unlock( fc::seconds(999999999), password );

      auto my_account1 = my_client->wallet_account_create( "account1" );
      my_client->wallet_import_private_key( test_keys[0], string(), true /*rescan*/ );
      auto bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      auto trx = my_client->wallet_account_register( "account1", "delegate30", variant(), true );

      auto your_account_key = your_client->wallet_account_create("youraccount");
      my_client->wallet_add_contact_account("youraccount", your_account_key);

      produce_block( my_client );

      for( uint32_t i = 0; i < 10; ++i )
      {
         my_client->wallet_transfer( 50000000+i, "XTS", "delegate30", "delegate30", "memo-"+fc::to_string(i) );
         produce_block( my_client );
      }
      my_client->wallet_submit_proposal("delegate30", "subject", "body", "type", fc::variant("data"));
      produce_block( my_client );
      for( uint32_t i = 0; i < 10; ++i )
      {
         my_client->wallet_transfer( 50000000+i, "XTS", "delegate30", "delegate30", "memo-"+fc::to_string(i) );
         produce_block( my_client );
      }
      my_client->wallet_vote_proposal("delegate30", 1, proposal_vote::yes, "I AGREE!!!");
      produce_block( my_client );
      my_cli->execute_command_line( "blockchain_list_proposals" );
      my_cli->execute_command_line( "blockchain_get_proposal_votes 1" ); 


   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }

}

struct test_file
{
       test_file(fc::path result_file, fc::path expected_result_file);

  bool perform_test(); //compare two files, return true if the files match
};
/*
char** CommandLineToArgvA(char* CmdLine, int* _argc)
{
  char** argv;
  char*  _argv;
  ULONG   len;
  ULONG   argc;
  CHAR   a;
  ULONG   i, j;

  BOOLEAN  in_QM;
  BOOLEAN  in_TEXT;
  BOOLEAN  in_SPACE;

  len = strlen(CmdLine);
  i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

  argv = (char**)GlobalAlloc(GMEM_FIXED,
      i + (len+2)*sizeof(CHAR));

  _argv = (char*)(((PUCHAR)argv)+i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = FALSE;
  in_TEXT = FALSE;
  in_SPACE = TRUE;
  i = 0;
  j = 0;

  while( a = CmdLine[i] ) {
      if(in_QM) {
          if(a == '\"') {
              in_QM = FALSE;
          } else {
              _argv[j] = a;
              j++;
          }
      } else {
          switch(a) {
          case '\"':
              in_QM = TRUE;
              in_TEXT = TRUE;
              if(in_SPACE) {
                  argv[argc] = _argv+j;
                  argc++;
              }
              in_SPACE = FALSE;
              break;
          case ' ':
          case '\t':
          case '\n':
          case '\r':
              if(in_TEXT) {
                  _argv[j] = '\0';
                  j++;
              }
              in_TEXT = FALSE;
              in_SPACE = TRUE;
              break;
          default:
              in_TEXT = TRUE;
              if(in_SPACE) {
                  argv[argc] = _argv+j;
                  argc++;
              }
              _argv[j] = a;
              j++;
              in_SPACE = FALSE;
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

BOOST_AUTO_TEST_CASE( separate_process_client_tests )
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
    std::ifstream test_config_file("testconfig");
    string line;
    while (std::getline(test_config_file,line))
    {
      //parse line into argc/argv format for boost program_options
      #ifdef UNIX
      //use wordexp
      #else
      //use ExpandEnvironmentStrings and CommandLineToArgvW
      #endif
      fc::path result_file;
      fc::path expected_result_file;
      tests.push_back( test_file(result_file,expected_result_file) );
    } //end while not eof
    for (test_file current_test : tests)
      current_test.perform_test();
  } catch ( const fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string() ) );
    throw;
  }
}

*/