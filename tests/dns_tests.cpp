#define BOOST_TEST_MODULE DNSTests
#include <boost/test/unit_test.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>
#include <fc/filesystem.hpp>
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

            auto next_block = _botwallet.generate_next_block( _db, txs );
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
        FC_ASSERT("!Expected exception");
    }
    catch (const fc::exception& e)
    {
        if (fail)
            throw;
        else
            return;
    }
}


/* You should not be able to start an auction for an invalid name (length)
 */
BOOST_AUTO_TEST_CASE( new_auction_name_length_fail )
{

}

/* You should be able to bid on a domain that is in an auction. The previous
 * owner should get the right amount of funds and the fee should be big enough
 */
BOOST_AUTO_TEST_CASE( bid_on_auction )
{

}

/* Your bid should fail if the domain is not in an auction
 * TODO does this duplicate "new_auction_for_unexpired_name_fail"? Do the txs look
 * different?
 */
BOOST_AUTO_TEST_CASE( bid_fail_not_in_auction )
{

}

/* Your bid should fail if the fee is not sufficient
 */
BOOST_AUTO_TEST_CASE( bid_fail_insufficient_fee )
{

}

/* Your bid should fail if you don't pay the previous owner enough
 */
BOOST_AUTO_TEST_CASE( bid_fail_prev_owner_payment )
{

}

/* You should be able to update a record if you still own the domain
 */
BOOST_AUTO_TEST_CASE( update_record )
{

}

/* You should not be able to update a record if you don't own the domain (wrong sig)
 */
BOOST_AUTO_TEST_CASE( update_record_sig_fail )
{

}

/* You should not be able to update a record if you don't own the domain (expired)
 */
BOOST_AUTO_TEST_CASE( update_record_expire_fail )
{

}

/* You should not be able to update a record with an invalid value (length)
 */
BOOST_AUTO_TEST_CASE( update_record_val_length_fail )
{

}

/* You should be able to sell a domain if you own it
 */
BOOST_AUTO_TEST_CASE( sell_domain )
{

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
