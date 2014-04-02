#define BOOST_TEST_MODULE DNSTests

#include <iostream>
#include <boost/test/unit_test.hpp>
#include <fc/filesystem.hpp>

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/pow_validator.hpp>
#include <bts/wallet/wallet.hpp>

#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/dns_util.hpp>

#define DNS_TEST_NUM_WALLET_ADDRS   10
#define DNS_TEST_BLOCK_SECS         (5 * 60)

#define DNS_TEST_NAME               "DNS_TEST_NAME"
#define DNS_TEST_VALUE              "DNS_TEST_VALUE"

#define DNS_TEST_BID1               asset(uint64_t(1))
#define DNS_TEST_BID2               asset(uint64_t(2))

using namespace bts::blockchain;
using namespace bts::dns;
using namespace bts::wallet;

trx_block generate_genesis_block( const std::vector<address>& addr )
{
    trx_block genesis;
    genesis.version           = 0;
    genesis.block_num         = 0;
    genesis.timestamp         = fc::time_point::now();
    genesis.next_fee          = block_header::min_fee();
    genesis.total_shares      = 0;

    signed_transaction trx;
    for( uint32_t i = 0; i < addr.size(); ++i )
    {
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
        genesis.total_shares += amnt;
    }
    genesis.trxs.push_back( trx );
    genesis.trx_mroot = genesis.calculate_merkle_root(signed_transactions());

    return genesis;
}

/* State for simulating a DNS chain. Two default wallets initialized at genesis */
class DNSTestState
{
    public:
        std::shared_ptr<sim_pow_validator>  validator;
        fc::ecc::private_key                auth;
        bts::dns::dns_db                    db;

        bts::dns::dns_wallet                wallet1;
        bts::dns::dns_wallet                wallet2;

        std::vector<address>                addrs1;
        std::vector<address>                addrs2;

        DNSTestState()
        {
            fc::temp_directory dir;

            validator = std::make_shared<sim_pow_validator>(fc::time_point::now());
            auth = fc::ecc::private_key::generate();

            db.set_trustee(auth.get_public_key());
            db.set_pow_validator(validator);
            db.open(dir.path() / "dns_test_db", true);

            wallet1.create(dir.path() / "dns_test_wallet1.dat", "password", "password", true);
            wallet2.create(dir.path() / "dns_test_wallet2.dat", "password", "password", true);

            addrs1 = std::vector<address>();
            addrs2 = std::vector<address>();

            /* Start the blockchain with random balances in new addresses */
            for (auto i = 0; i < DNS_TEST_NUM_WALLET_ADDRS; ++i)
            {
                addrs1.push_back(wallet1.new_recv_address());
                addrs2.push_back(wallet2.new_recv_address());
            }

            std::vector<address> addrs = addrs1;
            addrs.insert(addrs.end(), addrs2.begin(), addrs2.end());
            auto genblk = generate_genesis_block(addrs);
            genblk.sign(auth);
            db.push_block(genblk);

            wallet1.scan_chain(db);
            wallet2.scan_chain(db);
        }

        /* Put these transactions into a block */
        void next_block(dns_wallet &wallet, signed_transactions &txs)
        {
            validator->skip_time(fc::seconds(DNS_TEST_BLOCK_SECS));

            if (txs.size() <= 0)
            {
                auto tx = wallet.transfer(DNS_TEST_BID1, random_addr(wallet));
                txs.push_back(tx);
                //for (auto i = 0; i < 3; i++ )
                //{
                    //auto transfer_tx = _botwallet.transfer( asset(uint64_t(1000000)), random_addr() );
                    //txs.push_back( transfer_tx );
                //}
            }
            
            // need to have non-zero CDD output in block, DNS records don't add anything
            // TODO should they?

            auto next_block = wallet.generate_next_block(db, txs);
            next_block.sign(auth);
            db.push_block(next_block);

            wallet1.scan_chain(db);
            wallet2.scan_chain(db);
            txs.clear();
        }

        void next_block(signed_transactions &txs)
        {
            next_block(wallet1, txs);
        }

        /* Give this address funds from some genesis addresses */
        /*
        void top_up( bts::blockchain::address addr, uint64_t amount )
        {
            std::vector<signed_transaction> trxs;
            auto transfer_tx = _botwallet.transfer(asset(amount), addr);
            trxs.push_back( transfer_tx );
            next_block( trxs );
        }
        */

        /* Get a new address. The global test wallet will also have it. */
        /*
        bts::blockchain::address next_addr()
        {
            auto addr = _botwallet.new_recv_address();
            _addrs.push_back( addr );
            return addr;
        }
        */

        /* Get a random existing address. Good for avoiding dust in certain tests. */
        address random_addr(dns_wallet &wallet)
        {
            if (&wallet == &wallet1)
                return addrs1[random() % (addrs1.size())];
            else if (&wallet == &wallet2)
                return addrs2[random() % (addrs2.size())];

            throw;
        }

        address random_addr()
        {
            return random_addr(wallet1);
        }
};

/* 
 */
BOOST_AUTO_TEST_CASE (templ)
{
    try
    {
        /* All unit test logic here */
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should be able to start a new auction for a name that the network has never seen before */
BOOST_AUTO_TEST_CASE(new_auction_for_new_name)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should be able to start a new auction for an expired name */
BOOST_AUTO_TEST_CASE(new_auction_for_expired_name)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Let domain expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Bid on same domain from second wallet */
        tx = state.wallet2.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(state.wallet2, txs);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to start a new auction on a domain that exists and is not expired */
// TODO: need to test wallet and validator separately for all failure cases
BOOST_AUTO_TEST_CASE(new_auction_for_unexpired_name_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to bid on same domain from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to start an auction for an invalid name (length) */
BOOST_AUTO_TEST_CASE(new_auction_name_length_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid name */
        std::string name = "";
        for (int i = 0; i < DNS_MAX_NAME_LEN + 1; i++)
            name.append("A");

        /* Try to bid on name */
        auto no_exception = false;
        try
        {
            tx = state.wallet1.bid_on_domain(name, DNS_TEST_BID1, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should be able to bid on a domain that is in an auction. The previous
 * owner should get the right amount of funds and the fee should be big enough
 */
// TODO: this still passes when BID1 = BID2 = 1
BOOST_AUTO_TEST_CASE( bid_on_auction )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Bid on same domain from second wallet */
        tx = state.wallet2.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID2, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(state.wallet2, txs);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Your bid should fail if the domain is not in an auction
 * TODO does this duplicate "new_auction_for_unexpired_name_fail"? Do the txs look
 * different?
 */
// TODO: craft a transaction trying to bid on expired name but referencing it as input
BOOST_AUTO_TEST_CASE( bid_fail_not_in_auction )
{
}

// TODO: Manually craft transaction with a sufficient network fee and money back but too low auction fee
/* Your bid should fail if the fee is not sufficient
 */
BOOST_AUTO_TEST_CASE( bid_fail_insufficient_fee )
{
}

/* Your bid should fail if you don't pay the previous owner enough
 */
// TODO: Manually craft transaction with a sufficient network fee and auction fee but too low money back
BOOST_AUTO_TEST_CASE( bid_fail_prev_owner_payment )
{
}

/* You should be able to update a record if you still own the domain
 */
BOOST_AUTO_TEST_CASE( update_domain_record )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Update domain record */
        tx = state.wallet1.update_domain_record(DNS_TEST_NAME, DNS_TEST_VALUE, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to update a record if you are not the confirmed owner (still in auction) */
BOOST_AUTO_TEST_CASE( update_domain_record_in_auction_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Try to update record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1.update_domain_record(DNS_TEST_NAME, DNS_TEST_VALUE, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to update a record if you don't own the domain (wrong sig) */
BOOST_AUTO_TEST_CASE( update_domain_record_sig_fail )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to update record from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2.update_domain_record(DNS_TEST_NAME, DNS_TEST_VALUE, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to update a record if you don't own the domain (expired) */
BOOST_AUTO_TEST_CASE( update_domain_record_expire_fail )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Let domain expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to update record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1.update_domain_record(DNS_TEST_NAME, DNS_TEST_VALUE, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to update a record with an invalid value (length) */
BOOST_AUTO_TEST_CASE( update_domain_record_val_length_fail )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid value */
        std::string str = "";
        for (auto i = 0; i < DNS_MAX_VALUE_LEN + 1; i++)
            str.append("A");
        fc::variant value = str;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to update record with value */
        auto no_exception = false;
        try
        {
            tx = state.wallet1.update_domain_record(DNS_TEST_NAME, value, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should be able to sell a domain if you own it */
BOOST_AUTO_TEST_CASE( auction_domain )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Auction domain */
        tx = state.wallet1.auction_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to sell a domain if you don't own it (sig) */
BOOST_AUTO_TEST_CASE( auction_domain_sig_fail )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to auction domain from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2.auction_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* You should not be able to sell a domain if you don't own it (expired) */
BOOST_AUTO_TEST_CASE( auction_domain_expire_fail )
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial domain purchase */
        tx = state.wallet1.bid_on_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Let domain expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs); 

        /* Try to auction domain */
        auto no_exception = false;
        try
        {
            tx = state.wallet1.auction_domain(DNS_TEST_NAME, DNS_TEST_BID1, txs, state.db);

            no_exception = true;
            throw;
        }
        catch (const fc::exception &e)
        {
            if (no_exception)
                throw;
        }
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}
