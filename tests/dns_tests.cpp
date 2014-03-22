#define BOOST_TEST_MODULE DNSTests
#include <boost/test/unit_test.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>
#include <fc/filesystem.hpp>
#include <fc/reflect/variant.hpp>
#include <iostream>

#include <bts/blockchain/config.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/pow_validator.hpp>
#include <bts/wallet/wallet.hpp>

#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/dns_config.hpp>

using namespace bts::wallet;
using namespace bts::blockchain;
using namespace bts::dns;

trx_block generate_genesis_block( const std::vector<address>& addr )
{
    trx_block genesis;
    genesis.version           = 0;
    genesis.block_num         = 0;
    genesis.timestamp         = fc::time_point::now();
    genesis.next_fee          = block_header::min_fee();
    genesis.total_shares      = 0;
    genesis.votes_cast        = 0;
    genesis.noncea            = 0;
    genesis.nonceb            = 0;
    genesis.next_difficulty   = 0;

    signed_transaction trx;
    for( uint32_t i = 0; i < addr.size(); ++i )
    {
        uint64_t amnt = rand()%10000 * BTS_BLOCKCHAIN_SHARE;
        trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
        genesis.total_shares += amnt;
    }
    genesis.available_votes   = genesis.total_shares;
    genesis.trxs.push_back( trx );
    genesis.trx_mroot = genesis.calculate_merkle_root(signed_transactions());

    return genesis;
}

/* State for simulating a DNS chain. It has a default wallet which has the genesis ledger keys.
 */
class DNSTestState
{
    public:
        DNSTestState()
        {
            fc::temp_directory dir;
            _validator = std::make_shared<sim_pow_validator>( fc::time_point::now() );
            _auth = fc::ecc::private_key::generate();            
            _addrs = std::vector<bts::blockchain::address>();
            _addrs.reserve(100);
            _botwallet.create( dir.path() / "dns_test_wallet.dat", "password", "password", true );
            _db.set_signing_authority( _auth.get_public_key() );
            _db.set_pow_validator( _validator );
            _db.open( dir.path() / "dns_db", true );
        }
        /* Start the blockchain with random balances in new addresses */
        void normal_genesis()
        {
            for ( uint32_t i = 0; i < 100; ++i)
            {
                _addrs.push_back( _botwallet.new_recv_address() );
            }

            _db.push_block( generate_genesis_block( _addrs ) );

            auto head_id = _db.head_block_id();
            _db.set_block_signature( head_id, 
                _auth.sign_compact( fc::sha256::hash((char*)&head_id, sizeof(head_id)) ));
            _botwallet.scan_chain( _db );
        }
        /* Put these transactions into a block */
        void next_block( std::vector<signed_transaction> txs )
        {
            _validator->skip_time( fc::seconds(5 * 60) );
            
            // need to have non-zero CDD output in block, DNS records don't add anything
            // TODO should they?
            for (auto i = 0; i < 3; i++ )
            {
                auto transfer_tx = _botwallet.transfer( asset(uint64_t(1000000)), random_addr() );
                txs.push_back( transfer_tx );
            }

            int64_t miner_votes = 0;
            auto next_block = _botwallet.generate_next_block( _db, txs, miner_votes );
            _db.push_block( next_block );

            auto head_id = _db.head_block_id();
            _db.set_block_signature( head_id, 
                _auth.sign_compact( fc::sha256::hash((char*)&head_id, sizeof(head_id)) ));

            _botwallet.scan_chain( _db );
        }
        /* Give this address funds from some genesis addresses */
        void top_up( bts::blockchain::address addr, uint64_t amount )
        {
            std::vector<signed_transaction> trxs;
            auto transfer_tx = _botwallet.transfer(asset(amount), addr);
            trxs.push_back( transfer_tx );
            next_block( trxs );
        }
        bts::dns::dns_wallet* get_wallet()
        {
            return &_botwallet;
        }
        bts::dns::dns_db* get_db()
        {
            return &_db;
        }
        /* Get a new address. The global test wallet will also have it. */
        bts::blockchain::address next_addr()
        {
            auto addr = _botwallet.new_recv_address();
            _addrs.push_back( addr );
            return addr;
        }
        /* Get a random existing address. Good for avoiding dust in certain tests.
         */
        bts::blockchain::address random_addr()
        {
            return _addrs[random()%(_addrs.size())];
        }
    private:
        std::shared_ptr<bts::blockchain::sim_pow_validator>      _validator;
        std::vector<bts::blockchain::address>   _addrs;
        fc::ecc::private_key                    _auth;
        // the test state gets its own wallet, used by default
        bts::dns::dns_wallet                     _botwallet;
        bts::dns::dns_db                         _db;

};

/* 
 */
BOOST_AUTO_TEST_CASE ( templ )
{
    try {

    }
    catch (const fc::exception& e)
    {

        throw;
    }
}

/* You should be able to start a new auction for a name that the
 * network has never seen before
 */
BOOST_AUTO_TEST_CASE( new_auction_for_new_name )
{
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(uint64_t(1)), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        
        state.next_block( txs );
    }
    catch (const fc::exception& e)
    {
        elog( "${e}", ("e",e.to_detail_string()) );
        throw;
    }
}

/* You should be able to start a new auction for an expired name
 */
BOOST_AUTO_TEST_CASE( new_auction_for_expired_name )
{
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(uint64_t(1)), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        
        state.next_block( txs );
        std::vector<signed_transaction> empty_txs;
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
        {
            state.next_block( empty_txs ); 
        }

        auto buy_tx2 = wallet->buy_domain( "TESTNAME", asset(uint64_t(1)), *state.get_db() );
        empty_txs.push_back( buy_tx2 );
        state.next_block( empty_txs );
    }
    catch (const fc::exception& e)
    {
        elog( "${e}", ("e",e.to_detail_string()) );
        throw;
    }
}

/* You should not be able to start a new auction on a domain that exists
 * and is not expired
 */
BOOST_AUTO_TEST_CASE( new_auction_for_unexpired_name_fail )
{
    bool fail = false; // what is best practice for failing tests w/ exceptions?
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(uint64_t(1)), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        
        state.next_block( txs );
        std::vector<signed_transaction> empty_txs;
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS / 2; i++)
        {
            state.next_block( empty_txs ); 
        }

        auto buy_tx2 = wallet->buy_domain( "TESTNAME", asset(uint64_t(1)), *state.get_db() );
        wlog( "buy_trx_2: ${trx} ", ("trx", buy_tx2) );
        empty_txs.push_back( buy_tx2 );
        state.next_block( empty_txs );
        
        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* You should not be able to start an auction for an invalid name (length)
 */
BOOST_AUTO_TEST_CASE( new_auction_name_length_fail )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        // Build invalid name
        std::string name = "";
        for (int i = 0; i < BTS_DNS_MAX_NAME_LEN + 1; i++)
            name.append("A");

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;
        auto buy_tx = wallet->buy_domain(name, asset(uint64_t(1)), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        
        state.next_block( txs );
        
        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* You should be able to bid on a domain that is in an auction. The previous
 * owner should get the right amount of funds and the fee should be big enough
 */
BOOST_AUTO_TEST_CASE( bid_on_auction )
{
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;

        // Create second wallet
        fc::temp_directory dir;
        bts::dns::dns_wallet wallet2;
        wallet2.create( dir.path() / "dns_test_wallet2.dat", "password", "password", true );

        uint64_t bid1 = 1;
        uint64_t bid2 = 100;

        // Give second wallet a balance
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        auto transfer_tx = wallet->transfer(asset(amnt), wallet2.new_recv_address());
        wlog( "transfer_trx: ${trx} ", ("trx",transfer_tx) );
        txs.push_back( transfer_tx );
        state.next_block( txs );
        txs.clear();

        // Initial domain purchase
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(bid1), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Bid on auction from second wallet
        wallet2.scan_chain(*state.get_db());
        buy_tx = wallet2.buy_domain( "TESTNAME", asset(bid2), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
    }
    catch (const fc::exception& e)
    {
        elog( "${e}", ("e",e.to_detail_string()) );
        throw;
    }
}

/* Your bid should fail if the domain is not in an auction
 * TODO does this duplicate "new_auction_for_unexpired_name_fail"? Do the txs look
 * different?
 */
BOOST_AUTO_TEST_CASE( bid_fail_not_in_auction )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;

        // Create second wallet
        fc::temp_directory dir;
        bts::dns::dns_wallet wallet2;
        wallet2.create( dir.path() / "dns_test_wallet2.dat", "password", "password", true );

        uint64_t bid1 = 1;
        uint64_t bid2 = 100;

        // Give second wallet a balance
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        auto transfer_tx = wallet->transfer(asset(amnt), wallet2.new_recv_address());
        wlog( "transfer_trx: ${trx} ", ("trx",transfer_tx) );
        txs.push_back( transfer_tx );
        state.next_block( txs );
        txs.clear();

        // Initial domain purchase
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(bid1), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let auction expire
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        // Bid on auction from second wallet
        wallet2.scan_chain(*state.get_db());
        buy_tx = wallet2.buy_domain( "TESTNAME", asset(bid2), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* Your bid should fail if the fee is not sufficient
 */
BOOST_AUTO_TEST_CASE( bid_fail_insufficient_fee )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;
        // TODO: proper argument for get_balance?
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(wallet->get_balance(0)), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* Your bid should fail if you don't pay the previous owner enough
 */
BOOST_AUTO_TEST_CASE( bid_fail_prev_owner_payment )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        std::vector<signed_transaction> txs;

        // Create second wallet
        fc::temp_directory dir;
        bts::dns::dns_wallet wallet2;
        wallet2.create( dir.path() / "dns_test_wallet2.dat", "password", "password", true );

        uint64_t bid1 = 100;
        uint64_t bid2 = BTS_DNS_MIN_BID_FROM(bid1) - 1;

        // Give second wallet a balance
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        auto transfer_tx = wallet->transfer(asset(amnt), wallet2.new_recv_address());
        wlog( "transfer_trx: ${trx} ", ("trx",transfer_tx) );
        txs.push_back( transfer_tx );
        state.next_block( txs );
        txs.clear();

        // Initial domain purchase
        auto buy_tx = wallet->buy_domain( "TESTNAME", asset(bid1), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Bid on auction from second wallet
        wallet2.scan_chain(*state.get_db());
        buy_tx = wallet2.buy_domain( "TESTNAME", asset(bid2), *state.get_db() );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* You should be able to update a record if you still own the domain
 */
BOOST_AUTO_TEST_CASE( update_record )
{
    try {
        DNSTestState state;
        state.normal_genesis();

        // Buy domain
        std::string name = "TESTNAME";
        fc::variant value = "TESTVALUE";

        dns_wallet* wallet = state.get_wallet();
        dns_db* db = state.get_db();

        std::vector<signed_transaction> txs;

        auto buy_tx = wallet->buy_domain( name, asset(uint64_t(1)), *db );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let auction expire
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        auto update_tx = wallet->update_record(name, value, *db);
        wlog( "update_trx: ${trx} ", ("trx", update_tx) );
        txs.push_back( update_tx );
        state.next_block( txs );
    }
    catch (const fc::exception& e)
    {
        elog( "${e}", ("e",e.to_detail_string()) );
        throw;
    }
}

// TODO: see todo on line ~150 of dns_wallet.cpp
/* You should not be able to update a record if you are not the confirmed owner (still in auction)
 */
BOOST_AUTO_TEST_CASE( update_record_in_auction_fail)
{

}

/* You should not be able to update a record if you don't own the domain (wrong sig)
 */
BOOST_AUTO_TEST_CASE( update_record_sig_fail )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        bts::dns::dns_wallet* wallet = state.get_wallet();
        dns_db* db = state.get_db();
        std::vector<signed_transaction> txs;

        std::string name = "TESTNAME";
        fc::variant value = "TESTVALUE";

        // Create second wallet
        fc::temp_directory dir;
        bts::dns::dns_wallet wallet2;
        wallet2.create( dir.path() / "dns_test_wallet2.dat", "password", "password", true );

        // Give second wallet a balance
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        auto transfer_tx = wallet->transfer(asset(amnt), wallet2.new_recv_address());
        wlog( "transfer_trx: ${trx} ", ("trx",transfer_tx) );
        txs.push_back( transfer_tx );
        state.next_block( txs );
        txs.clear();

        // Initial domain purchase
        uint64_t bid1 = 1;
        auto buy_tx = wallet->buy_domain( name, asset(bid1), *db );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let auction expire
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        // Attempt to update record from second wallet
        wallet2.scan_chain(*state.get_db());
        auto update_tx = wallet2.update_record(name, value, *db);
        wlog( "update_trx: ${trx} ", ("trx", update_tx) );
        txs.push_back( update_tx );
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* You should not be able to update a record if you don't own the domain (expired)
 */
BOOST_AUTO_TEST_CASE( update_record_expire_fail )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        // Buy domain
        std::string name = "TESTNAME";
        fc::variant value = "TESTVALUE";

        dns_wallet* wallet = state.get_wallet();
        dns_db* db = state.get_db();

        std::vector<signed_transaction> txs;

        auto buy_tx = wallet->buy_domain( name, asset(uint64_t(1)), *db );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let record expire
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        auto update_tx = wallet->update_record(name, value, *db);
        wlog( "update_trx: ${trx} ", ("trx", update_tx) );
        txs.push_back( update_tx );
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

BOOST_AUTO_TEST_CASE( update_record_name_length_fail )
{
}

/* You should not be able to update a record with an invalid value (length)
 */
BOOST_AUTO_TEST_CASE( update_record_val_length_fail )
{
    bool fail = false; // what is best practice for 
    try {
        DNSTestState state;
        state.normal_genesis();

        // Buy domain
        std::string name = "TESTNAME";
        std::string str = "";
        for (auto i = 0; i < BTS_DNS_MAX_VALUE_LEN + 1; i++)
            str.append("A");
        fc::variant value = str;

        dns_wallet* wallet = state.get_wallet();
        dns_db* db = state.get_db();

        std::vector<signed_transaction> txs;

        auto buy_tx = wallet->buy_domain( name, asset(uint64_t(1)), *db );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let auction expire
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        auto update_tx = wallet->update_record(name, value, *db);
        wlog( "update_trx: ${trx} ", ("trx", update_tx) );
        txs.push_back( update_tx );
        state.next_block( txs );

        fail = true;
        FC_ASSERT(0);
    }
    catch (const fc::exception& e)
    {
        if (fail)
        {
            elog( "${e}", ("e",e.to_detail_string()) );
            throw;
        }
    }
}

/* You should be able to sell a domain if you own it
 */
BOOST_AUTO_TEST_CASE( sell_domain )
{
    try {
        DNSTestState state;
        state.normal_genesis();

        dns_wallet* wallet = state.get_wallet();
        dns_db* db = state.get_db();

        // Buy domain
        std::string name = "TESTNAME";
        std::vector<signed_transaction> txs;
        asset price = uint64_t(1);

        auto buy_tx = wallet->buy_domain( name, price, *db );
        wlog( "buy_trx: ${trx} ", ("trx",buy_tx) );
        txs.push_back( buy_tx );
        state.next_block( txs );
        txs.clear();

        // Let auction expire
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        auto sell_tx = wallet->sell_domain(name, price, *db);
        wlog( "sell_trx: ${trx} ", ("trx", sell_tx) );
        txs.push_back( sell_tx );
        state.next_block( txs );
    }
    catch (const fc::exception& e)
    {
        elog( "${e}", ("e",e.to_detail_string()) );
        throw;
    }
}

/* You should not be able to sell a domain if you don't own it (sig)
 */
BOOST_AUTO_TEST_CASE( sell_domain_sig_fail )
{

}

/* You should not be able to sell a domain if you don't own it (expired)
 */
BOOST_AUTO_TEST_CASE( sell_domain_expire_fail )
{

}

BOOST_AUTO_TEST_CASE( sell_domain_name_length_fail )
{
}
