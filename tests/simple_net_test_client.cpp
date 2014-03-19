#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <bts/net/node.hpp>
#include <bts/client/messages.hpp>

#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/filesystem.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>

#include <iostream>
using namespace bts::wallet;
using namespace bts::blockchain;


trx_block generate_genesis_block(const std::vector<address>& addr)
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
        uint64_t amnt = rand()%1000 * BTS_BLOCKCHAIN_SHARE;
        trx.outputs.push_back( trx_output( claim_by_signature_output( addr[i] ), asset( amnt ) ) );
        genesis.total_shares += amnt;
    }
    genesis.available_votes   = genesis.total_shares;
    genesis.trxs.push_back( trx );
    genesis.trx_mroot = genesis.calculate_merkle_root(signed_transactions());

    return genesis;
}


using namespace bts::net;
using namespace bts::client;
using namespace boost::multi_index;

class simple_net_test_miner : public bts::net::node_delegate
{
private:
  struct block_hash{};
  typedef boost::multi_index_container<block_message, indexed_by<random_access<>, 
                                                                 ordered_unique<tag<block_hash>, member<block_message, block_id_type, &block_message::block_id> > 
                                                                 ordered_unique<tag<message_hash>, member   > > ordered_blockchain_container;
  ordered_blockchain_container blockchain;

  typedef boost::multi_index_container<block_message, indexed_by<ordered_unique<member<block_message, block_id_type, &block_message::block_id> > > > blockchain_container;
  blockchain_container unsigned_blocks;

  struct transaction_hash{};
  struct chain_length{};
  struct transaction_with_timestamp
  {
    trx_message transaction_message;
    transaction_id_type transaction_id;
    uint32_t chain_length_when_received;
  };
  typedef boost::multi_index_container<transaction_with_timestamp, indexed_by<ordered_unique<tag<transaction_hash>, member<transaction_with_timestamp, transaction_id_type, &transaction_with_timestamp::transaction_id> >, 
                                                                              ordered_non_unique<tag<chain_length>, member<transaction_with_timestamp, uint32_t, &transaction_with_timestamp::chain_length_when_received> > > > transaction_container;
  transaction_container transactions;

  struct signature_hash{};
  typedef boost::multi_index_container<signature_message, indexed_by<ordered_unique<tag<transaction_hash>, member<transaction_with_timestamp, transaction_id_type, &transaction_with_timestamp::transaction_id> >, 
                                                                              ordered_non_unique<tag<chain_length>, member<transaction_with_timestamp, uint32_t, &transaction_with_timestamp::chain_length_when_received> > > > transaction_container;
  

  bool synchronizing;
  void push_valid_signed_block(const block_message& block_to_push);
  bool is_block_valid(const block_message& block_to_check);
  void store_transaction(const trx_message& transaction_to_store);
public:


  bool has_item(const item_id& id) override;
  void handle_message(const message&) override;
  std::vector<item_hash_t> get_item_ids(const item_id& from_id, 
                                        uint32_t& remaining_item_count,
                                        uint32_t limit = 2000) override;
  message get_item(const item_id& id) override; 
  void sync_status(uint32_t item_type, uint32_t item_count) override;
  void connection_count_changed(uint32_t c) override;
};


void simple_net_test_miner::push_valid_signed_block(const block_message& block_to_push)
{
  blockchain.push_back(block_to_push);
  // any unsigned blocks we have are garbage
  unsigned_blocks.clear();
  // prune old transactions from memory
  if (blockchain.size() > 2)
    transactions.get<chain_length>().erase(transactions.get<chain_length>().begin(), transactions.get<chain_length>().lower_bound(blockchain.size() - 2));
}

bool simple_net_test_miner::is_block_valid(const block_message& block_to_check)
{
  if (blockchain.empty())
    return true; // no way of knowing 
  block_to_check.block.prev;
  ordered_blockchain_container::index<block_hash>::type::iterator iter = blockchain.get<block_hash>().find(block_to_check.block.prev);
  if (iter != blockchain.get<block_hash>().end())
    return true;
  return false;
}

void simple_net_test_miner::store_transaction(const trx_message& transaction_to_store)
{
  transaction_with_timestamp transaction_info;
  transaction_info.transaction_message = transaction_to_store;
  transaction_info.transaction_id = transaction_to_store.trx.id(); 
  transaction_info.chain_length_when_received = blockchain.size();
  transactions.insert(transaction_info);
}


bool simple_net_test_miner::has_item(const item_id& id)
{
  if (id.item_type == trx_message_type)
    return transactions.get<transaction_hash>().find(id.item_hash) != transactions.end();
  if (id.item_type == block_message_type)
      return blockchain.get<block_hash>().find(id.item_hash) != blockchain.get<block_hash>().end();
  return false;
}

void simple_net_test_miner::handle_message(const message& message_to_handle)
{
  switch (message_to_handle.msg_type)
  {
  case block_message_type:
    {
      block_message block_message_to_handle(message_to_handle.as<block_message>());
      if (!is_block_valid(block_message_to_handle))
        FC_THROW("Invalid block received, I want to disconnect from the peer that sent it");
      if (block_message_to_handle.signature != fc::ecc::compact_signature())
        push_valid_signed_block(block_message_to_handle);
      else
        unsigned_blocks.insert(block_message_to_handle);
      break;
    }
  case trx_message_type:
    {
      trx_message trx_message_to_handle(message_to_handle.as<trx_message>());
      store_transaction(trx_message_to_handle);
      break;
    }
  case signature_message_type:
    {
      signature_message signature_message_to_handle(message_to_handle.as<signature_message>());

    }
  }
}

std::vector<item_hash_t> simple_net_test_miner::get_item_ids(const item_id& from_id, 
                                                             uint32_t& remaining_item_count,
                                                             uint32_t limit)
{
  ordered_blockchain_container::index<block_hash>::type::iterator block_hash_iter = blockchain.get<block_hash>().find(from_id.item_hash);
  if (block_hash_iter == blockchain.get<block_hash>().end())
  {
    assert(false); // should this ever happen?
    FC_THROW("I've never seen your last item");
    remaining_item_count = 0;
    return std::vector<item_hash_t>();
  }
  ordered_blockchain_container::nth_index<0>::type::iterator sequential_iter = blockchain.project<0>(block_hash_iter);
  ++sequential_iter;
  remaining_item_count = blockchain.get<0>().end() - sequential_iter;
  uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
  std::vector<item_hash_t> hashes_to_return;
  hashes_to_return.reserve(items_to_get_this_iteration);
  for (uint32_t i = 0; i < items_to_get_this_iteration; ++i)
  {
    hashes_to_return.push_back(sequential_iter->block_id);
    ++sequential_iter;
  }
  remaining_item_count -= items_to_get_this_iteration;
  return hashes_to_return;
}

message simple_net_test_miner::get_item(const item_id& id) 
{
  if (id.item_type == trx_message_type)
  {
    transaction_container::index<transaction_hash>::type::iterator iter = transactions.get<transaction_hash>().find(id.item_hash);
    if (iter != transactions.get<transaction_hash>().end())
      return iter->transaction_message;
  }
  if (id.item_type == block_message_type)
  {
    ordered_blockchain_container::index<block_hash>::type::iterator iter = blockchain.get<block_hash>().find(id.item_hash);
    if (iter != blockchain.get<block_hash>().end())
      return *iter;
  }
  FC_THROW("I don't have the item you're looking for");
}

int main()
{
  return 0;
}