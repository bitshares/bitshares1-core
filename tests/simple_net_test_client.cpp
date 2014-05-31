#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <boost/program_options.hpp>

#include <bts/net/node.hpp>
#include <bts/client/messages.hpp>

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/filesystem.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>

using namespace bts::blockchain;
using namespace bts::net;
using namespace bts::client;

// Generate a fake starting block.
// This block has all elements default-initialized, except for the block
// number which is 1.  (chosen because we'll consider a signature valid if
// it is equal to the block number, and we treat a signature of 0 to mean
// no signature.
block_message generate_fake_genesis_block()
{
  block_message genesis_block;
  genesis_block.block.block_num = 1;
  genesis_block.block_id = genesis_block.block.id();
  (*(uint32_t*)&genesis_block.signature.at(0)) = 1;
  ilog("generated genesis block, its id is ${id}", ("id", genesis_block.block_id));
  return genesis_block;
}


using namespace boost::multi_index;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
class blockchain_tied_message_cache
{
   private:
     static const uint32_t cache_duration_in_blocks = 2;

     struct message_hash_index{};
     struct block_clock_index{};
     struct message_info
     {
       message_hash_type message_hash;
       message           message_body;
       uint32_t          block_clock_when_received;
       message_info(const message_hash_type& message_hash,
                    const message&           message_body,
                    uint32_t                 block_clock_when_received) :
         message_hash(message_hash),
         message_body(message_body),
         block_clock_when_received (block_clock_when_received)
       {}
     };
     typedef boost::multi_index_container<message_info, indexed_by<ordered_unique<tag<message_hash_index>, member<message_info, message_hash_type, &message_info::message_hash> >,
                                                                   ordered_non_unique<tag<block_clock_index>, member<message_info, uint32_t, &message_info::block_clock_when_received> > > > message_cache_container;
     message_cache_container _message_cache;

     uint32_t block_clock; //
   public:
     blockchain_tied_message_cache() :
       block_clock(0)
     {}
     void block_accepted();
     void cache_message(const message& message_to_cache, const message_hash_type& hash_of_message_to_cache);
     message get_message(const message_hash_type& hash_of_message_to_lookup);
};

void blockchain_tied_message_cache::block_accepted()
{
  ++block_clock;
  if (block_clock > cache_duration_in_blocks)
    _message_cache.get<block_clock_index>().erase(_message_cache.get<block_clock_index>().begin(),
                                                 _message_cache.get<block_clock_index>().lower_bound(block_clock - cache_duration_in_blocks));
}

void blockchain_tied_message_cache::cache_message(const message& message_to_cache, const message_hash_type& hash_of_message_to_cache)
{
  _message_cache.insert(message_info(hash_of_message_to_cache, message_to_cache, block_clock));
}

message blockchain_tied_message_cache::get_message(const message_hash_type& hash_of_message_to_lookup)
{
  message_cache_container::index<message_hash_index>::type::const_iterator iter = _message_cache.get<message_hash_index>().find(hash_of_message_to_lookup);
  if (iter != _message_cache.get<message_hash_index>().end())
    return iter->message_body;
  FC_THROW_EXCEPTION(key_not_found_exception, "Requested message not in cache");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
class simple_net_test_miner : public bts::net::node_delegate
{
   private:
     node _node;

     // cache of messages we've seen in the past few blocks
     blockchain_tied_message_cache _message_cache;

     // blockchain is a series of block_messages
     struct block_hash_index{};
     typedef boost::multi_index_container<block_message, indexed_by<random_access<>,
                                                                    ordered_unique<tag<block_hash_index>, member<block_message, block_id_type, &block_message::block_id> > > > ordered_blockchain_container;
     ordered_blockchain_container _blockchain;

     // the list of unsigned blocks we're considering adding to our chain
     typedef boost::multi_index_container<block_message, indexed_by<ordered_unique<member<block_message, block_id_type, &block_message::block_id> > > > block_container;
     block_container _unsigned_blocks;

     // the list of transactions we're considering adding to a new block if we mine one.
     // we expire these after a few blocks.  A real client would also expire them immediately
     // if it received a block containing the transaction
     struct transaction_hash_index{};
     struct chain_length_index{};
     struct transaction_with_timestamp
     {
       bts::blockchain::signed_transaction transaction;
       transaction_id_type                 transaction_id;
       uint32_t                            chain_length_when_received;
     };
     typedef boost::multi_index_container<transaction_with_timestamp, indexed_by<ordered_unique<tag<transaction_hash_index>, member<transaction_with_timestamp, transaction_id_type, &transaction_with_timestamp::transaction_id> >,
                                                                                 ordered_non_unique<tag<chain_length_index>, member<transaction_with_timestamp, uint32_t, &transaction_with_timestamp::chain_length_when_received> > > > transaction_container;
     transaction_container _transactions;

     // the list of signatures we've seen but have been unable to pair with a block (maybe the unsigned block hasn't arrived yet).
     typedef boost::multi_index_container<signature_message, indexed_by<ordered_unique<member<signature_message, block_id_type, &signature_message::block_id> > > > signature_container;
     signature_container _signatures;

     bool _synchronized;
     uint32_t _remaining_items_to_sync;


     uint16_t _listening_port;
     fc::path _config_dir;

     void push_valid_signed_block(const block_message& block_to_push);
     bool is_block_valid(const block_message& block_to_check);
     void store_transaction(const trx_message& transaction_to_store);

   public:
     simple_net_test_miner();
     void set_port(uint16_t port);
     void set_config_dir(const std::string& config_dir);
     void connect_to(fc::ip::endpoint remote_endpoint);
     void start();
     void generate_signed_block();
     void run();

     /* Implement node_delegate */
     bool has_item(const item_id& id) override;
     void handle_message(const message&) override;
     std::vector<item_hash_t> get_item_ids(const item_id& from_id,
                                           uint32_t& remaining_item_count,
                                           uint32_t limit = 2000) override;
     message get_item(const item_id& id) override;
     void sync_status(uint32_t item_type, uint32_t item_count) override;
     void connection_count_changed(uint32_t c) override;
};

simple_net_test_miner::simple_net_test_miner() :
  _synchronized(false),
  _remaining_items_to_sync(0),
  _listening_port(6543)
{
  _node.set_node_delegate(this);
  _config_dir = fc::app_path() / "simple_net_test_miner";
  push_valid_signed_block(generate_fake_genesis_block());
}

void simple_net_test_miner::set_port(uint16_t port)
{
  _listening_port = port;
}

void simple_net_test_miner::set_config_dir(const std::string& config_dir)
{
  _config_dir = config_dir.c_str();
}

void simple_net_test_miner::start()
{
  FC_ASSERT(!_blockchain.empty());
  _synchronized = false;

  if (!fc::exists(_config_dir))
    fc::create_directories(_config_dir);

  fc::file_appender::config appender_config;
  appender_config.filename = _config_dir / "log.txt";
  appender_config.truncate = false;
  appender_config.flush = true;
  fc::logger::get().add_appender(fc::shared_ptr<fc::file_appender>(new fc::file_appender(fc::variant(appender_config))));

  _node.load_configuration(_config_dir);

  _node.sync_from(item_id(block_message_type, _blockchain.back().block_id));
  _node.listen_on_port(_listening_port);
  _node.connect_to_p2p_network();
}
void simple_net_test_miner::connect_to(fc::ip::endpoint remote_endpoint)
{
  _node.connect_to(remote_endpoint);
}

void simple_net_test_miner::generate_signed_block()
{
  // generate a block
  block_message new_block;
  int last_block_number = _blockchain.get<0>().back().block.block_num;
  ilog("mined block ${block_number}", ("block_number", last_block_number + 1));
  new_block.block.block_num = last_block_number + 1;
  new_block.block.prev = _blockchain.get<0>().back().block_id;
  new_block.block_id = new_block.block.id();
  (*(uint32_t*)&new_block.signature.at(0)) = last_block_number + 1;
  push_valid_signed_block(new_block);
  message new_block_message(new_block);
  _message_cache.cache_message(new_block_message, new_block_message.id());
  _node.broadcast(new_block_message);
}

void simple_net_test_miner::run()
{
  if (_listening_port == 6540)
    for (int i = 0; i < 410; ++i)
      generate_signed_block();

  srand((unsigned)time(NULL));
  for (;;)
  {
    // sleep for 10-20 seconds
    fc::usleep(fc::milliseconds(15000 + ((rand() % 10000) - 5000)));
    ilog("looping");
    if (_listening_port == 6540)
      generate_signed_block();
  }
}


void simple_net_test_miner::push_valid_signed_block(const block_message& block_to_push)
{
  auto push_result = _blockchain.push_back(block_to_push);
  FC_ASSERT(push_result.second); // assert that insert succeeded

  // any unsigned blocks we have are garbage
  _unsigned_blocks.clear();

  // prune list of cached messages
  _message_cache.block_accepted();

  // prune list of uncommitted _transactions
  unsigned cached_transaction_duration_in_blocks = 2;
  if (_blockchain.size() > cached_transaction_duration_in_blocks)
    _transactions.get<chain_length_index>().erase(_transactions.get<chain_length_index>().begin(),
                                                  _transactions.get<chain_length_index>().lower_bound(_blockchain.size() - cached_transaction_duration_in_blocks));

  if (!_synchronized)
  {
    --_remaining_items_to_sync;
    if (_remaining_items_to_sync == 0)
      _synchronized = true;
  }

  ilog("CLIENT: pushed block ${id}, blockchain is now ${count} blocks long", ("id", block_to_push.block_id)("count", _blockchain.size()));
}

bool simple_net_test_miner::is_block_valid(const block_message& block_to_check)
{
  FC_ASSERT(!_blockchain.empty()); // we should always start with the genesis block
  if (_blockchain.empty())
    return true; // no way of knowing
  //block_to_check.block.prev;
  ordered_blockchain_container::index<block_hash_index>::type::iterator iter = _blockchain.get<block_hash_index>().find(block_to_check.block.prev);
  if (iter != _blockchain.get<block_hash_index>().end())
    return true;
  wlog("Got invalid block ${block}.  It's prev hash is ${prev} so I'm calling it invalid",
       ("block",block_to_check.block_id)("prev",block_to_check.block.prev));
  wlog("Last block on the block chain is ${id}",("id", _blockchain.back().block_id));
  return false;
}

void simple_net_test_miner::store_transaction(const trx_message& transaction_to_store)
{
  transaction_with_timestamp transaction_info;
  transaction_info.transaction = transaction_to_store.trx;
  transaction_info.transaction_id = transaction_to_store.trx.id();
  transaction_info.chain_length_when_received = _blockchain.size();
  _transactions.insert(transaction_info);
}

bool simple_net_test_miner::has_item(const item_id& id)
{
  ilog("CLIENT: node asks if I have item type ${type} hash ${hash}",("type", id.item_type)("hash", id.item_hash));
  if (id.item_type == trx_message_type)
    return _transactions.get<transaction_hash_index>().find(id.item_hash) != _transactions.end();
  if (id.item_type == block_message_type)
  {
    ilog("CLIENT: ... I answer: ${have_it}",("have_it",_blockchain.get<block_hash_index>().find(id.item_hash) != _blockchain.get<block_hash_index>().end()));
    return _blockchain.get<block_hash_index>().find(id.item_hash) != _blockchain.get<block_hash_index>().end();
  }
  return false;
}

void simple_net_test_miner::handle_message(const message& message_to_handle)
{
  switch (message_to_handle.msg_type)
  {
  case block_message_type:
    {
      block_message block_message_to_handle(message_to_handle.as<block_message>());
      ilog("CLIENT: just received block ${id}", ("id", block_message_to_handle.block_id));
      if (!is_block_valid(block_message_to_handle))
        FC_THROW("Invalid block received, I want to disconnect from the peer that sent it");
      if (block_message_to_handle.signature != fc::ecc::compact_signature())
        push_valid_signed_block(block_message_to_handle);
      else
      {
        // do I have a signature for it?
        signature_container::iterator iter = _signatures.find(block_message_to_handle.block_id);
        if (iter != _signatures.end())
        {
          // it's signed!
          block_message_to_handle.signature = iter->signature;
          _signatures.erase(iter);
          push_valid_signed_block(block_message_to_handle);
        }
        else
          _unsigned_blocks.insert(block_message_to_handle);
      }
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
      // do we have a block for it?
      block_container::iterator iter = _unsigned_blocks.find(signature_message_to_handle.block_id);
      if (iter != _unsigned_blocks.end())
      {
        block_message signed_block_message = *iter;
        _unsigned_blocks.erase(iter);
        signed_block_message.signature = signature_message_to_handle.signature;
        push_valid_signed_block(signed_block_message);
      }
      else
        _signatures.insert(signature_message_to_handle);
      break;
    }
  }

  // only cache the message if it was valid (we didn't throw)
  _message_cache.cache_message(message_to_handle, message_to_handle.id());
}

std::vector<item_hash_t> simple_net_test_miner::get_item_ids(const item_id& from_id,
                                                             uint32_t& remaining_item_count,
                                                             uint32_t limit)
{
  // artificially limit to 50 ids per message during testing
  limit = 50;

  ordered_blockchain_container::index<block_hash_index>::type::iterator block_hash_iter = _blockchain.get<block_hash_index>().find(from_id.item_hash);
  if (block_hash_iter == _blockchain.get<block_hash_index>().end())
  {
    remaining_item_count = 0;
    return std::vector<item_hash_t>();
  }
  ordered_blockchain_container::nth_index<0>::type::iterator sequential_iter = _blockchain.project<0>(block_hash_iter);
  ++sequential_iter;
  remaining_item_count = _blockchain.get<0>().end() - sequential_iter;
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
  try
  {
    message result = _message_cache.get_message(id.item_hash);
    ilog("get_item() returning message from _message_cache (id: ${item_hash})", ("item_hash", result.id()));
    ilog("item's real hash is ${hash}", ("hash", fc::ripemd160::hash(&result.data[0], result.data.size())));
    return result;
  }
  catch (const fc::key_not_found_exception&)
  {
    // not in our cache.  Either it has already expired from our cache, or
    // it's a request for an actual block during synchronization.
  }

  if (id.item_type == block_message_type)
  {
    ordered_blockchain_container::index<block_hash_index>::type::iterator iter = _blockchain.get<block_hash_index>().find(id.item_hash);
    if (iter != _blockchain.get<block_hash_index>().end())
    {
      ilog("get_item() treating id as block_id, should only happen during sync");
      return *iter;
    }
  }

  FC_THROW_EXCEPTION(key_not_found_exception, "I don't have the item you're looking for");
}

void simple_net_test_miner::sync_status(uint32_t item_type, uint32_t item_count)
{
  _remaining_items_to_sync = item_count;
  _synchronized = _remaining_items_to_sync == 0;
  if (!item_count)
    ilog("TEST CLIENT: I am now in sync");
}

void simple_net_test_miner::connection_count_changed(uint32_t c)
{
  ilog("TEST CLIENT: I now have ${count} connections", ("count", c));
}

int main(int argc, char* argv[])
{
  boost::program_options::options_description option_config("Allowed options");
  option_config.add_options()("port", boost::program_options::value<uint16_t>(), "set port to listen on")
                             ("connect-to", boost::program_options::value<std::string>(), "set remote host to connect to")
                             ("config-dir", boost::program_options::value<std::string>(), "directory containing config files");
  boost::program_options::variables_map option_variables;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, option_config), option_variables);
  boost::program_options::notify(option_variables);

  simple_net_test_miner miner;

  if (option_variables.count("config-dir"))
    miner.set_config_dir(option_variables["config-dir"].as<std::string>());

  if (option_variables.count("port"))
    miner.set_port(option_variables["port"].as<uint16_t>());

  miner.start();

  if (option_variables.count("connect-to"))
    miner.connect_to(fc::ip::endpoint::from_string(option_variables["connect-to"].as<std::string>()));

  miner.run();
  return 0;
}
