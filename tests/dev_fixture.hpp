#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/genesis_state.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/cli/cli.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <bts/utilities/deterministic_openssl_rand.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/http/connection.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>

#include <fc/log/logger.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>

#define BTS_BLOCKCHAIN_INITIAL_SHARES (BTS_BLOCKCHAIN_MAX_SHARES/5)

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::utilities;
using namespace bts::client;
using namespace bts::cli;

struct chain_fixture
{
   chain_fixture()
   :console( new fc::console_appender() )
   { try {
      ilog( "." );

      try {
      sim_network = std::make_shared<bts::net::simulated_network>("dev_fixture");

      bts::blockchain::start_simulated_time(fc::time_point::from_iso_string( "20200101T000000" ));
      bts::utilities::set_random_seed_for_testing( fc::sha512() );

      //console.configure( {"",console_appender::stream::std_error,{},true} );

      delegate_private_keys =
        fc::json::from_string( R"(["3feb0fde257e07abce682c924289d62bdd20b2b4e4c7381a9b1e1c587da26a50",
                                "b217221a26039680a407fd08281b75e15564edec82210b40aeda459ad5252a19",
                                "72a41ecabead3111909509cf9297255ef88b649d5a124e7e5d1fc4f97248aac5",
                                "f6588d2c58476fddd9d8b060d90b5bf2855cbc34a7a25f9d9a7e8a82e49e2ebe",
                                "29e4abede459b1226ca80f2e3f1d51537bb8d5013c8a10eafdbf7290252f8a7c",
                                "9ec8990e117159f1fe366c5bd4747bee68d78584865f7fb76295430729c3b199",
                                "00572a843472f311c7096afb1e3cb64180c5db39100cf59028387305aaaabfac",
                                "e38e8754b1876a017232d48278b5a7c04d988684ff8e7aadb8cbed669b7ab3ae",
                                "237f1236895b853b284286ff6e4ab94231d841db8896f154831b058c39902911",
                                "5b8cfaddf5b52aaebc2bd6cb461c84522118a26761881a212a0f2b8b84575b60",
                                "0028d51da3c7cffd70a99417854c7e40ffc70a87deb89a5a95199f20bf197745",
                                "390ead266aa95badda0bba3419f9131e0c274074565f9b1c43d59f324b632ae6",
                                "5c22447abbef7955daf0cd82f581c7c52bf7b9830f225102775676a47553f332",
                                "8a9f969c82bce41d2b82fc577f6d9097e277226546281897c42cea991bf92f9f",
                                "7584635ec5953f3840b874899c0e30ae99b0bad8ca5114a706f1355cf75ca652",
                                "442a639a1abf91cd22f5078293731f7350b2dcac2858f675702b2a558da29ef1",
                                "11a355a08db1fffb92ed492503cc964f89235c44a035f1fffd6f1b3365ff76bb",
                                "dcf6b3dc79f97cd44019f9fa40dfe9d0f3088eabfd1d319f46db951bc3d5f4aa",
                                "ae91f4ab8e129cbefe228540e517d6324eb3b244223d96c7ceffd6460dc82283",
                                "69a1aa48cc5d8c93807ea09a708e5f1f91abc3d47d89bf698c71e4c5b7253baa",
                                "6f34c1d0bb4f1ab31cdaf8cedae37ee023dbdf680adcbb0c6922b2b37becbc30",
                                "062b7153861ad228b3d7ae22e0500c651d5a870f51a256565c692a5f4bccde4d",
                                "b04b091ce8a0f05833768954ba48ccf282928c3d660f9f767bd24179a42248e7",
                                "6b41cdfbbf74b08b33770fb37db101271e2ef4c749e85263321478f7c02cbd80",
                                "d979cd57e5d17c4df1fea5f42b77efed5dfe3b3cc4e4ddc6fd841c63ada04194",
                                "6d1acd733223255279cdf694df80ade96660bac313e00c913c79333aff972d04",
                                "36e340b626975b53ace709404920bbc2c1c95050b23186251a7a8a6fc5d2d7d1",
                                "2058be88702b97159bf54a51df9f9416ebb078dd119012d671609296678fc783",
                                "c2ec01876e162109d16d796c64b0f61322b457e0750d556805acbe0055c14e61",
                                "4af015fe9bacaeef8862d2086165c77f82ce6cebce1462170ad3981987d18cb9",
                                "8d95da1c0ef0e49b4e6fb7c58b4c23e29f9694d9a0a0273d5b3ed31feb9fc94b",
                                "f2943ce0c94166e7201088a3186f8dbf282001d636e6bbca27c9ab7c50658714",
                                "f7c37340bc33a8eb60d5ae8531485ff2a9f65c35083b73d7055125398f43f22d",
                                "5d0a8cd12d8ad831a5f06ac017638c81d521517b2d88567ef62a4176f171b536",
                                "195e8c06cdc0bdfa86621a940400ad162b509d0701726b31f0aaf9198f8123fa",
                                "168adf4d6a27cb07cc14aa7767c337e8354430c6bf3db7d5c9ab903c7bfaaa8d",
                                "9b78adad2b2fbae31474cd36a3dfd3df3526cf3b423881903d72a75ab739addf",
                                "259523b4e12c85e62bf5f2bc926b711f063282468c8266bae5de6a1a911e72b4",
                                "7e87e0e0be61cb43e019ab5450256347205b14da8ae942c8e6789a79defcc242",
                                "61588a7c7d6e41f19c2f3b5d68e25eb575602c7b9c84d3d3f1be793c0f6a86d7",
                                "09c81566a57fabf09ef806656f08ba56b5ceac42151014e953b539e645bb2136",
                                "6e7eeb7ca44f5312474013f057354980741c87717503b09470ce3501550b4cbd",
                                "e7e8496ad86d06a1de94fab03ba6727415b5f92469f76d128f706a152dd06060",
                                "4acb4c124c86873e70291f016169530f739e608e874eefefe7c7276b96b839cb",
                                "a7a379a31058a101cf9f42496d63318771712559379d10dca7248f07d7aa5779",
                                "f2eb71b2100a917da2518f0a0cbbcef330e63b1081cdaca42cdeecda9ddebb65",
                                "ed624dc352e86970a5963ebbcad217449f0c18794028a8f41120b31bb8821407",
                                "9682caaf9c86b0a27d11cbdf913ffe5c8a9ea1e11af1aa4610e4582e30c400df",
                                "5f810206658cb0db6dbb78fb3181154ec8bb5f3f3750dd347dc5e92d8264b757",
                                "d0d0f8ac39f5f2c7bf5d2b6286bf8d4e6fad54d98d0ff55e16cbd43830a50dda",
                                "3745521dd7737698a04542031cd222155fa0d3441771976a680be4cd9a7c9e92",
                                "e67c82e562082b4be3f846fdf934594a720914e955ba0b235e7ae5449b45151b",
                                "14ec83cc8515c1e56b05301df49c7054b5d567bc48882d3297b71028c4ba9d1a",
                                "449f5324f6d6364c20fa330d66eac4eafe5463ca9a7e06d02be14d7f8b21b1c2",
                                "1c935e2ea27974d6484f1e5d765b63e33ae0654141f1dc3b4c14ff8a6684a507",
                                "3307a77d98632cd043180b6c232f2638a4e46369e31a9676188c647632173cfa",
                                "e6dcb52f032048c8b52eb8c478db6e76619d672502daba2c16220aefade0e039",
                                "bf4f87f6aab24d9c6685ce92523dd5d6e511c0f0fe97b9c67ad45089593dd362",
                                "f2be971a9b9338800dec77b054781b7ef4f62946ab52ddaa4d3985578ca87070",
                                "b7781bf514db2306478e491fb0a7c3492445b2851dc44f7dbb331eeb9f7a46cc",
                                "d383d66c02ca36eb90450e16933555f6a0b0513e3e8ae800d2eef9dd88f1db1f",
                                "d1f2b7c2f4bf94b656c8783a14e42a1e297908ae1c5e45aeb55e72b576965c58",
                                "5e5de16f16869bcebc9c186d6198764ffb71076104475dfc37950b6fad3de017",
                                "8a7311a6dd6bfb27ff8a789d60be21b74ad0dab1267f9716f9004e1d01aeb040",
                                "17c3b577622d2dad0710ea574c19e5357ec3011956ff689d6e17f13d909bbc81",
                                "4ef7fc48655043554a856b617afce1a8f9d6486a309f45f74dd0fc95d52dfcd5",
                                "b766d75f2b4f14653e48274c61c8ce771d3c67d3d271bb8849b445a9e6f3e770",
                                "a901888e5df002ac5dd6849c6cd7a52ae58292e6d3d48b667ee3d54355963542",
                                "03d580b961bed8a52792a4c91a5ed31caf4bd14045f5007b081ef210bda7b166",
                                "e566e4ef300a66abcb7efdbc557032685eda6ddde65cbb7665ef83f674ad6369",
                                "89a6e210fd6842ada2854e669ee48ddd52aa5eaf3096ee7a532e2840e2d1aa7d",
                                "924c4d5409a79c2d8a64e92ac2d30d1a789fbb47bf4d6e2e9ad901c58db013b5",
                                "f77e1cc3d855af30b9f3ad66b7449fa66c7e65223d1582d6e2dc9e4a96b8fac9",
                                "50ca9b69ea30080bc59a11a7b972a8e0458ca7b6dcec92d544dde36c52a3d7dc",
                                "21ef878c7e6fd74380833187758b8b4475b04c734ce7c83018bea28aaf244ba3",
                                "5abd4816614a27d151ce8d5b914f96bdff0997bc4034e870607d3eace368b149",
                                "ebb5e1c74580198fbae07b3409a65d3825519cbf7f5e35062cba3596866b73f1",
                                "82b21ed109a2279cfa82f673997a30303909d467e2a2ce45aaa4c0e9e865818d",
                                "9623f0a51af76d4a58c719d08691f9c58dbc808aa0982c708a3368818d2fa7c5",
                                "85b14a32f761bba494b4c043ab84974f6e1f6243148132579eb21f871cc5994d",
                                "ae4e0cb7b5f987cfa6564bce7e8de03139c645b487cbaa58ab4668c4982a2c30",
                                "544397f1cca1304d5428b77a94708d67fd7d19608b53a83ef66105472d6d6480",
                                "50c1f1861df8b4900162baef55d24ced3d9b8c7154ab79be63252bfba0091e1e",
                                "595027b699c201d8c562fcbc0c6b84c21568204149b88d2e238c75d6bd6fdd66",
                                "099b912b6243efa9c04acbcd5f88bc1019847d1f0011099c51a85d08196a2400",
                                "4dc4cb741bde87391dfabb33574cd047ab5e4d7dede618691b12e2cc8fde3e67",
                                "5f5ac65a9b79515accdd9424131d3e6a7bbdfe495b2726c0a87c6ce8d0d8edcd",
                                "e816781df566b1c58a225d74b090051f23391d394affd79dfd1712817a05b77d",
                                "5ff87283ea6261a5bae670ff569e3ab702d1b52a48a1b6e62023237aea909eed",
                                "fa5d590adf7d46743e5d98c6578f9b9a622e50793497b55317deaa25b3089d86",
                                "4d3d8b130c580fa173a98ff227e173b3e4a34e76859b9c045fac96deb22a467b",
                                "c53c2499ce7c5173278cfc5dbc04f09441bfa13f1eecb678cdc8d4ca977cc2a1",
                                "22e01cf32f35d81d472808d0c683889ae59aef422a7bc83f2588a674642df2ee",
                                "c02084227247bbf6341c7aea7f93d71b75cca8061d9d85d68f17dc8912009dcb",
                                "e73ed687f42c8bbf306e7fbc806f7526e01011b61deb66b222aa981640c2b67e",
                                "ca0517f7475722af9f585f7ff068b39bcb3c018860c25360ba9370dca8cc352f",
                                "ca0517f7475722af9f585f7ff068b39bcb3c018860c25360ba9370dca8cc352a",
                                "ca0517f7475722af9f585f7ff068b39bcb3c018860c25360ba9370dca8cc352b",
                                "ca0517f7475722af9f585f7ff068b39bcb3c018860c25360ba9370dca8cc352c",
                                "ca0517f7475722af9f585f7ff068b39bcb3c018860c25360ba9370dca8cc352d",
                                "a3331c76c729494f18a479f4c9a5b17d936ec89fca24b6287b8a84ce1aea60a8"])").as<vector< fc::ecc::private_key>>();

      genesis_state config;
      config.timestamp = bts::blockchain::now();

      for( uint32_t i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
      {
         genesis_delegate delegate_account;
         delegate_account.name = "delegate" + fc::to_string(i);
         auto delegate_public_key = delegate_private_keys[i].get_public_key();
         delegate_account.owner = delegate_public_key;
         config.delegates.push_back( delegate_account );

         genesis_balance balance;
         balance.raw_address = pts_address( fc::ecc::public_key_data( delegate_account.owner ) );
         balance.balance = double( BTS_BLOCKCHAIN_INITIAL_SHARES ) / BTS_BLOCKCHAIN_NUM_DELEGATES;
         config.initial_balances.push_back( balance );
      }

      fc::json::save_to_file( config, clienta_dir.path() / "genesis.json" );
      fc::json::save_to_file( config, clientb_dir.path() / "genesis.json" );

      clienta = std::make_shared<bts::client::client>("dev_fixture", sim_network);
      clienta->open( clienta_dir.path(), clienta_dir.path() / "genesis.json" );
      clienta->configure_from_command_line( 0, nullptr );
      clienta->set_daemon_mode(true);
      _clienta_done = clienta->start();
      ilog( "... " );

      clientb = std::make_shared<bts::client::client>("dev_fixture", sim_network);
      clientb->open( clientb_dir.path(), clientb_dir.path() / "genesis.json" );
      clientb->configure_from_command_line( 0, nullptr );
      clientb->set_daemon_mode(true);
      _clientb_done = clientb->start();
      ilog( "... " );

      enable_logging();
      exec(clienta, "wallet_create walleta \"masterpassword\" 123456ddddaxxx123456789012345678901234567890");
      exec(clienta, "wallet_set_automatic_backups false");
      exec(clienta, "wallet_set_transaction_scanning true");
      exec(clienta, "wallet_unlock 99999999999 masterpassword");

      exec(clientb, "wallet_create walletb \"masterpassword\" 123456a123456789012345678901234567890");
      exec(clientb, "wallet_set_automatic_backups false");
      exec(clientb, "wallet_set_transaction_scanning true");
      exec(clientb, "wallet_unlock 99999999999 masterpassword");

      int even = 0;
      for( auto key : delegate_private_keys )
      {
         if( (even++)%2 )
         {
             exec( clienta, "wallet_import_private_key " + key_to_wif( key  ) );
         }
         else
         {
             exec( clientb, "wallet_import_private_key " + key_to_wif( key  ) );
         }
      }

      } catch ( const fc::exception& e )
      {
         std::cerr << "exception: " << e.to_detail_string() <<"\n";
         edump( (e.to_detail_string() ) );
         throw;
      }
   } FC_LOG_AND_RETHROW() }

   template<typename T>
   void produce_block( T my_client )
   {
      fc::usleep( fc::microseconds( 10000 ) );
      if( my_client == clienta )
         console->print( "A: produce block----------------------------------------\n", fc::console_appender::color::blue );
      else
         console->print( "B: produce block----------------------------------------\n", fc::console_appender::color::green );

      auto head_num = my_client->get_chain()->get_head_block_num();
      const auto& delegates = my_client->get_wallet()->get_my_delegates( enabled_delegate_status | active_delegate_status );
      auto next_block_time = my_client->get_wallet()->get_next_producible_block_timestamp( delegates );
      FC_ASSERT( next_block_time.valid(), "", ("delegates",delegates) );
      bts::blockchain::advance_time( (int32_t)((*next_block_time - bts::blockchain::now()).count()/1000000) );
      auto b = my_client->get_chain()->generate_block(*next_block_time);
      my_client->get_wallet()->sign_block( b );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );
      idump( (b) );
      fc::usleep( fc::microseconds( 200000 ) );
      FC_ASSERT( head_num+1 == my_client->get_chain()->get_head_block_num() );
      bts::blockchain::advance_time( 7 );
   }

   void enable_logging()
   {
      fc::configure_logging( fc::logging_config::default_config() );
   }

   void disable_logging()
   {
      fc::logging_config cfg;
      fc::configure_logging( cfg );
   }

   ~chain_fixture()
   {
     clienta.reset();
     clientb.reset();
   }

   void exec( std::shared_ptr<bts::client::client> c, const string& command_to_run )
   {
      if( c == clienta )
         console->print( "A: " + command_to_run + "\n", fc::console_appender::color::blue );
      else
         console->print( "B: " + command_to_run + "\n", fc::console_appender::color::green );

      std::cerr << command_to_run << "\n";
      std::cerr << c->execute_command_line( command_to_run ) << "\n";
   }

   std::shared_ptr<bts::net::simulated_network> sim_network;
   std::shared_ptr<bts::net::simulated_network> sim_network_fork;
   std::shared_ptr<bts::client::client>        clienta;
   fc::future<void>                            _clienta_done;
   std::shared_ptr<bts::client::client>        clientb;
   fc::future<void>                            _clientb_done;
   fc::temp_directory             clienta_dir;
   fc::temp_directory             clientb_dir;
   vector<fc::ecc::private_key>   delegate_private_keys;
   fc::console_appender*          console;
};

