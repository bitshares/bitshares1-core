#define BOOST_TEST_MODULE DNSTests

#include <iostream>
#include <boost/test/unit_test.hpp>

#include <fc/filesystem.hpp>
#include <bts/blockchain/pow_validator.hpp>

#include <bts/dns/dns_db.hpp>
#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/p2p/p2p_transaction_validator.hpp>

#define DNS_TEST_NUM_WALLET_ADDRS   10
#define DNS_TEST_BLOCK_SECS         (5 * 60)

#define DNS_TEST_KEY                "DNS_TEST_KEY"
#define DNS_TEST_VALUE              "DNS_TEST_VALUE"

#define DNS_TEST_PRICE1             asset(uint64_t(1))
#define DNS_TEST_PRICE2             asset(uint64_t(2))

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::dns;
using namespace bts::dns::p2p;

trx_block generate_genesis_block( const std::vector<address>& addr )
{
    trx_block genesis;
    genesis.version           = 0;
    genesis.block_num         = 0;
    genesis.timestamp         = fc::time_point::now();
    genesis.next_fee          = block_header::min_fee();
    genesis.total_shares      = 0;

    signed_transaction dtrx;
    dtrx.vote = 0;
    // create initial delegates
    for( uint32_t i = 0; i < 100; ++i )
    {
       auto name     = "delegate-"+fc::to_string( int64_t(i+1) );
       auto key_hash = fc::sha256::hash( name.c_str(), name.size() );
       auto key      = fc::ecc::private_key::regenerate(key_hash);
       dtrx.outputs.push_back( trx_output( claim_name_output( name, std::string(), i+1, key.get_public_key(), key.get_public_key() ), asset() ) );
    }
    genesis.trxs.push_back( dtrx );

    // generate an initial genesis block that evenly allocates votes among all
    // delegates.
    for( uint32_t i = 0; i < 100; ++i )
    {
       signed_transaction trx;
       trx.vote = i + 1;
       for( uint32_t o = 0; o < 5; ++o )
       {
          uint64_t amnt = 200000;
          trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
          genesis.total_shares += amnt;
       }
       genesis.trxs.push_back( trx );
    }

    genesis.trx_mroot = genesis.calculate_merkle_root(signed_transactions());

    return genesis;
}

/* State for simulating a DNS chain. Two default wallets initialized at genesis */
class DNSTestState
{
    public:
        std::shared_ptr<sim_pow_validator>  validator;
        fc::ecc::private_key                auth;
        fc::path                            path;

        bts::dns::dns_db_ptr                db;

        bts::dns::dns_wallet_ptr            wallet1;
        bts::dns::dns_wallet_ptr            wallet2;

        std::vector<address>                addrs1;
        std::vector<address>                addrs2;

        DNSTestState()
        {
            validator = std::make_shared<sim_pow_validator>(fc::time_point::now());
            auth = fc::ecc::private_key::generate();
            fc::temp_directory dir;
            path = dir.path();

            db = std::make_shared<bts::dns::dns_db>();
            db->set_trustee(auth.get_public_key());
            db->set_pow_validator(validator);
            db->set_transaction_validator(std::make_shared<bts::dns::p2p::p2p_transaction_validator>(db.get()));
            db->open(path / "dns_test_db", true);

            wallet1 = std::make_shared<bts::dns::dns_wallet>(db);
            wallet2 = std::make_shared<bts::dns::dns_wallet>(db);

            wallet1->create_internal(path / "dns_test_wallet1.dat", "password", "password", true);
            wallet2->create_internal(path / "dns_test_wallet2.dat", "password", "password", true);

            addrs1 = std::vector<address>();
            addrs2 = std::vector<address>();

            /* Start the blockchain with random balances in new addresses */
            for (auto i = 0; i < DNS_TEST_NUM_WALLET_ADDRS; ++i)
            {
                addrs1.push_back(wallet1->new_receive_address());
                addrs2.push_back(wallet2->new_receive_address());
            }

            std::vector<address> addrs = addrs1;
            addrs.insert(addrs.end(), addrs2.begin(), addrs2.end());
            auto genblk = generate_genesis_block(addrs);
            genblk.sign(auth);
            db->push_block(genblk);

            wallet1->scan_chain(*db);
            wallet2->scan_chain(*db);
        }

        ~DNSTestState()
        {
            db->close();
            fc::remove_all(path);
        }

        /* Put these transactions into a block */
        void next_block(dns_wallet_ptr &wallet, signed_transactions &txs)
        {
            validator->skip_time(fc::seconds(DNS_TEST_BLOCK_SECS));

            if (txs.size() <= 0)
                txs.push_back(wallet->send_to_address(asset(1), random_addr(wallet)));

            auto next_block = wallet->generate_next_block(*db, txs);
            next_block.sign(auth);
            db->push_block(next_block);

            wallet1->scan_chain(*db);
            wallet2->scan_chain(*db);
            txs.clear();
        }

        void next_block(signed_transactions &txs)
        {
            next_block(wallet1, txs);
        }

        /* Get a random existing address. Good for avoiding dust in certain tests. */
        address random_addr(dns_wallet_ptr &wallet)
        {
            if (&wallet == &wallet1)
                return addrs1[rand() % (addrs1.size())];
            else if (&wallet == &wallet2)
                return addrs2[rand() % (addrs2.size())];

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

/* Current Tests:
 *
 * validator_bid_on_new
 * validator_bid_on_auction
 * validator_bid_on_expired
 * wallet_and_validator_bid_on_new
 * wallet_and_validator_bid_on_auction
 * wallet_and_validator_bid_on_expired
 *
 * validator_bid_on_auction_insufficient_bid_price_fail
 * validator_bid_on_owned_fail
 * wallet_bid_on_auction_insufficient_bid_price_fail
 * wallet_bid_on_owned_fail
 *
 * validator_bid_invalid_key_fail
 * validator_bid_insufficient_funds_fail
 * validator_bid_pending_txs_conflict_fail
 * wallet_bid_invalid_key_fail
 * wallet_bid_insufficient_funds_fail
 * wallet_bid_pending_txs_conflict_fail
 *
 * /// TODO: Merge transfer and update tests
 * validator_update
 * wallet_and_validator_update
 *
 * validator_update_in_auction_fail
 * validator_update_not_owner_fail
 * validator_update_expired_fail
 * wallet_update_in_auction_fail
 * wallet_update_not_owner_fail
 * wallet_update_expired_fail
 *
 * validator_update_invalid_key_fail
 * validator_update_invalid_value_fail
 * validator_update_pending_txs_conflict_fail
 * wallet_update_invalid_key_fail
 * wallet_update_invalid_value_fail
 * wallet_update_pending_txs_conflict_fail
 *
 * validator_transfer
 * wallet_and_validator_transfer
 *
 * validator_transfer_in_auction_fail
 * validator_transfer_not_owner_fail
 * validator_transfer_expired_fail
 * wallet_transfer_in_auction_fail
 * wallet_transfer_not_owner_fail
 * wallet_transfer_expired_fail
 *
 * validator_transfer_invalid_key_fail
 * validator_transfer_pending_txs_conflict_fail
 * wallet_transfer_invalid_key_fail
 * wallet_transfer_pending_txs_conflict_fail
 *
 * validator_auction
 * wallet_and_validator_auction
 *
 * validator_auction_in_auction_fail
 * validator_auction_not_owner_fail
 * validator_auction_expired_fail
 * wallet_auction_in_auction_fail
 * wallet_auction_not_owner_fail
 * wallet_auction_expired_fail
 *
 * validator_auction_invalid_key_fail
 * validator_auction_pending_txs_conflict_fail
 * wallet_auction_invalid_key_fail
 * wallet_auction_pending_txs_conflict_fail
 */

/* Validator shall accept a bid for a key that the network has never seen before */
BOOST_AUTO_TEST_CASE(validator_bid_on_new)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transaction tx;

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr();
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        auto bid_price = DNS_TEST_PRICE1;
        std::unordered_set<address> req_sigs;

        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet1->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall accept a bid for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(validator_bid_on_auction)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        auto bid_price = DNS_TEST_PRICE2;
        std::unordered_set<address> req_sigs;

        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        tx.inputs.push_back(trx_input(prev_tx_ref));

        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto transaction_validator = std::dynamic_pointer_cast<dns_transaction_validator>(state.db->get_transaction_validator());
        FC_ASSERT(transaction_validator != nullptr);
        auto transfer_amount = transaction_validator->get_bid_transfer_amount(bid_price, prev_output.amount);

        auto prev_dns_output = to_dns_output(prev_output);
        tx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), transfer_amount));
        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet2->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall accept a bid for a key that is expired */
BOOST_AUTO_TEST_CASE(validator_bid_on_expired)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        auto bid_price = DNS_TEST_PRICE1;
        std::unordered_set<address> req_sigs;

        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet2->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can create a bid for a key that the network has never seen before */
BOOST_AUTO_TEST_CASE(wallet_and_validator_bid_on_new)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Bid on dns */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can create a bid for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(wallet_and_validator_bid_on_auction)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Bid on same dns from second wallet */
        tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE2, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can create a bid for a key that is expired */
BOOST_AUTO_TEST_CASE(wallet_and_validator_bid_on_expired)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Bid on same dns from second wallet */
        tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall not accept a bid for a key that is being auctioned without exceeding the previous bid by a
 * sufficient amount */
BOOST_AUTO_TEST_CASE (validator_bid_on_auction_insufficient_bid_price_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE2, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        auto bid_price = DNS_TEST_PRICE1; /* Lower bid */
        std::unordered_set<address> req_sigs;

        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        tx.inputs.push_back(trx_input(prev_tx_ref));

        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto transfer_amount = 0u;

        auto prev_dns_output = to_dns_output(prev_output);
        tx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), transfer_amount));
        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet2->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a bid for a key that has already been auctioned and is not expired */
BOOST_AUTO_TEST_CASE(validator_bid_on_owned_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        auto bid_price = DNS_TEST_PRICE2;
        std::unordered_set<address> req_sigs;

        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        tx.inputs.push_back(trx_input(prev_tx_ref));

        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto transaction_validator = std::dynamic_pointer_cast<dns_transaction_validator>(state.db->get_transaction_validator());
        FC_ASSERT(transaction_validator != nullptr);
        auto transfer_amount = transaction_validator->get_bid_transfer_amount(bid_price, prev_output.amount);

        auto prev_dns_output = to_dns_output(prev_output);
        tx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), transfer_amount));
        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet2->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create a bid for a key that is being auctioned without exceeding the previous bid by a
 * sufficient amount */
BOOST_AUTO_TEST_CASE (wallet_bid_on_auction_insufficient_bid_price_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE2, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Try to bid on key from second wallet with insufficient bid price */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE1 , txs);

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

/* Wallet shall not create a bid for a key that has already been auctioned and is not expired */
BOOST_AUTO_TEST_CASE(wallet_bid_on_owned_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to bid on same dns from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE2, txs);

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

/* Validator shall not accept a bid for a key that has an invalid length */
BOOST_AUTO_TEST_CASE(validator_bid_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = key; /* Invalid key */
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr();
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        auto bid_price = DNS_TEST_PRICE1;
        std::unordered_set<address> req_sigs;

        tx.outputs.push_back(trx_output(dns_output, bid_price));

        tx = state.wallet1->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a bid for a key with a price exceeding available inputs */
BOOST_AUTO_TEST_CASE (validator_bid_insufficient_funds_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transaction tx;

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr();
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        auto bid_price = DNS_TEST_PRICE1;
        std::unordered_set<address> req_sigs;

        tx.outputs.push_back(trx_output(dns_output, state.wallet1->get_balance(0))); /* Invalid amount */

        tx = state.wallet1->collect_inputs_and_sign(tx, bid_price, req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a bid for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (validator_bid_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        validator->evaluate(tx, block_state);

        /* Different dns bid */
        tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create a bid for a key that has an invalid length */
BOOST_AUTO_TEST_CASE(wallet_bid_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Try to bid on key */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->bid(key, DNS_TEST_PRICE1, txs);

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

/* Wallet shall not create a bid for a key with a price exceeding available inputs */
BOOST_AUTO_TEST_CASE (wallet_bid_insufficient_funds_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Try to bid on key with an excessive bid price */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->bid(DNS_TEST_KEY, state.wallet1->get_balance(0), txs);

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

/* Wallet shall not create a bid for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (wallet_bid_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);

        /* Try to bid on same dns from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);

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

/* Validator shall accept an update for a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(validator_update)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can create an update for a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(wallet_and_validator_update)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Update dns record */
        tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall not accept an update for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(validator_update_in_auction_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an update for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(validator_update_not_owner_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(state.random_addr(state.wallet2));

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet2->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an update for a key that has expired */
BOOST_AUTO_TEST_CASE(validator_update_expired_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create an update for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(wallet_update_in_auction_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);

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

/* Wallet shall not create an update for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(wallet_update_not_owner_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to update dns record from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);

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

/* Wallet shall not create an update for a key that has expired */
BOOST_AUTO_TEST_CASE(wallet_update_expired_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);

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

/* Validator shall not accept an update for a key of invalid length */
BOOST_AUTO_TEST_CASE (validator_update_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = key;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an update for a key with a value of invalid length */
BOOST_AUTO_TEST_CASE(validator_update_invalid_value_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid value */
        std::string str = "";
        for (auto i = 0; i < DNS_MAX_VALUE_LEN + 1; i++)
            str.append("A");
        fc::variant value = str;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(value);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an update for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (validator_update_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Auction dns */
        tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        validator->evaluate(tx, block_state);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = serialize_value(DNS_TEST_VALUE);
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create an update for a key of invalid length */
BOOST_AUTO_TEST_CASE (wallet_update_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->set(key, DNS_TEST_VALUE, txs);

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

/* Wallet shall not create an update for a key with a value of invalid length */
BOOST_AUTO_TEST_CASE(wallet_update_invalid_value_fail)
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

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to update dns record with value */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->set(DNS_TEST_KEY, value, txs);

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

/* Wallet shall not create an update for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (wallet_update_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Auction dns */
        tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);

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

/* Validator shall accept a transfer for a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(validator_transfer)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can create a transfer for a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(wallet_and_validator_transfer)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Update dns record */
        tx = state.wallet1->transfer(DNS_TEST_KEY, state.random_addr(state.wallet2), txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall not accept a transfer for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(validator_transfer_in_auction_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a transfer for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(validator_transfer_not_owner_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(state.random_addr(state.wallet2));

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet2->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a transfer for a key that has expired */
BOOST_AUTO_TEST_CASE(validator_transfer_expired_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create a transfer for a key that is being auctioned */
BOOST_AUTO_TEST_CASE(wallet_transfer_in_auction_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->transfer(DNS_TEST_KEY, state.random_addr(state.wallet2), txs);

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

/* Wallet shall not create a transfer for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(wallet_transfer_not_owner_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to update dns record from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->transfer(DNS_TEST_KEY, state.random_addr(state.wallet2), txs);

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

/* Wallet shall not create a transfer for a key that has expired */
BOOST_AUTO_TEST_CASE(wallet_transfer_expired_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->transfer(DNS_TEST_KEY, state.random_addr(state.wallet2), txs);

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

/* Validator shall not accept a transfer for a key of invalid length */
BOOST_AUTO_TEST_CASE (validator_transfer_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = key;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept a transfer for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (validator_transfer_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Auction dns */
        tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        validator->evaluate(tx, block_state);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = state.random_addr(state.wallet2);
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::update;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, prev_output.amount));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create a transfer for a key of invalid length */
BOOST_AUTO_TEST_CASE (wallet_transfer_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->transfer(key, state.random_addr(state.wallet2), txs);

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

/* Wallet shall not create a transfer for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (wallet_transfer_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Auction dns */
        tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);

        /* Try to update dns record */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->transfer(DNS_TEST_KEY, state.random_addr(state.wallet2), txs);

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

/* Validator shall accept an auction for a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(validator_auction)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Wallet can auction a key that has been acquired and is not expired */
BOOST_AUTO_TEST_CASE(wallet_and_validator_auction)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Auction dns */
        tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Validate transaction */
        validator->evaluate(tx, block_state);
    }
    catch (const fc::exception &e)
    {
        std::cerr << e.to_detail_string() << "\n";
        elog("${e}", ("e", e.to_detail_string()));
        throw;
    }
}

/* Validator shall not accept an auction for a key that is being auctioned */
BOOST_AUTO_TEST_CASE (validator_auction_in_auction_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an auction for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(validator_auction_not_owner_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(state.random_addr(state.wallet2));

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet2->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an auction for a key that has expired */
BOOST_AUTO_TEST_CASE(validator_auction_expired_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create an auction for a key that is being auctioned */
BOOST_AUTO_TEST_CASE (wallet_auction_in_auction_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Try to auction dns */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);

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

/* Wallet shall not create an auction for a key that has been acquired by another owner */
BOOST_AUTO_TEST_CASE(wallet_auction_not_owner_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to auction dns from second wallet */
        auto no_exception = false;
        try
        {
            tx = state.wallet2->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);

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

/* Wallet shall not create an auction for a key that has expired */
BOOST_AUTO_TEST_CASE(wallet_auction_expired_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Let dns expire */
        for (auto i = 0; i < DNS_EXPIRE_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Try to auction dns */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);

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

/* Validator shall not accept an auction for a key of invalid length */
BOOST_AUTO_TEST_CASE (validator_auction_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = key;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Validator shall not accept an auction for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (validator_auction_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        auto validator = state.db->get_transaction_validator();
        auto block_state = validator->create_block_state();
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Update dns record */
        tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        validator->evaluate(tx, block_state);

        /* Get previous output */
        auto prev_tx_ref = state.db->get_dns_ref(DNS_TEST_KEY);
        auto prev_output = state.db->fetch_output(prev_tx_ref);
        auto prev_dns_output = to_dns_output(prev_output);

        /* Build dns output */
        claim_dns_output dns_output;
        dns_output.key = DNS_TEST_KEY;
        dns_output.value = std::vector<char>();
        dns_output.owner = prev_dns_output.owner;
        dns_output.last_tx_type = claim_dns_output::last_tx_type_enum::auction;

        /* Build full transaction */
        tx = signed_transaction();
        std::unordered_set<address> req_sigs;
        req_sigs.insert(prev_dns_output.owner);

        tx.inputs.push_back(trx_input(prev_tx_ref));
        tx.outputs.push_back(trx_output(dns_output, DNS_TEST_PRICE1));

        tx = state.wallet1->collect_inputs_and_sign(tx, asset(), req_sigs);
        wlog("tx: ${tx} ", ("tx", tx));

        /* Try to validate transaction */
        auto no_exception = false;
        try
        {
            validator->evaluate(tx, block_state);

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

/* Wallet shall not create an auction for a key of invalid length */
BOOST_AUTO_TEST_CASE (wallet_auction_invalid_key_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Build invalid key */
        std::string key = "";
        for (int i = 0; i < DNS_MAX_KEY_LEN + 1; i++)
            key.append("A");

        /* Try to auction dns */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->ask(key, DNS_TEST_PRICE1, txs);

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

/* Wallet shall not create an auction for a key when there already exists a transaction for the same key in the
 * current transaction pool */
BOOST_AUTO_TEST_CASE (wallet_auction_pending_txs_conflict_fail)
{
    try
    {
        DNSTestState state;
        signed_transactions txs;
        signed_transaction tx;

        /* Initial dns bid */
        tx = state.wallet1->bid(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);
        state.next_block(txs);

        /* Let auction end */
        for (auto i = 0; i < DNS_AUCTION_DURATION_BLOCKS; i++)
            state.next_block(txs);

        /* Update dns record */
        tx = state.wallet1->set(DNS_TEST_KEY, DNS_TEST_VALUE, txs);
        wlog("tx: ${tx} ", ("tx", tx));
        txs.push_back(tx);

        /* Try to auction dns */
        auto no_exception = false;
        try
        {
            tx = state.wallet1->ask(DNS_TEST_KEY, DNS_TEST_PRICE1, txs);

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
