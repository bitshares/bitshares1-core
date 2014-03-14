#define BOOST_TEST_MODULE DNSTests
#include <boost/test/unit_test.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger.hpp>
#include <fc/filesystem.hpp>
#include <iostream>

#include <bts/blockchain/config.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>

#include <bts/dns/dns_wallet.hpp>


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
    genesis.avail_coindays    = 0;
    genesis.noncea            = 0;
    genesis.nonceb            = 0;
    genesis.next_difficulty   = 0;

    signed_transaction trx;
    for( uint32_t i = 0; i < addr.size(); ++i )
    {
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
        genesis.total_shares += amnt;
    }
    genesis.trxs.push_back( trx );
    genesis.trx_mroot = genesis.calculate_merkle_root();

    return genesis;
}

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
        fc::temp_directory     dir;
        dns_wallet             wlt;
        std::vector<address>   addrs;
        dns_db                 dns_db;
        auto sim_validator = std::make_shared<sim_pow_validator>( &dns_db );

        wlt.create( dir.path() / "wallet.dat", "password", "password", true );
        auto addr = wlt.new_recv_address();
        addrs.push_back( addr );

        dns_db.set_pow_validator( sim_validator );
        dns_db.open( dir.path() / "dns_db", true);
        dns_db.push_block( generate_genesis_block( addrs ) );

        wlt.scan_chain( dns_db );
        wlt.set_stake( dns_db.get_stake(), dns_db.get_stake2() );
        wlt.set_fee_rate( dns_db.get_fee_rate() );
        
        auto buy_tx = wlt.buy_domain( "TESTNAME", asset(uint64_t(1)), dns_db );
        std::vector<signed_transaction> txs;
        txs.push_back( buy_tx );
        auto next_block = dns_db.generate_next_block( txs );

        wlt.scan_chain( dns_db );

    }
    catch (const fc::exception& e)
    {

        throw;
    }

}

/* You should be able to start a new auction for an expired name
 */
BOOST_AUTO_TEST_CASE( new_auction_for_expired_name )
{

}

/* You should not be able to start a new auction on a domain that exists
 * and is not expired
 */
BOOST_AUTO_TEST_CASE( new_auction_for_unexpired_name_fail )
{

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
