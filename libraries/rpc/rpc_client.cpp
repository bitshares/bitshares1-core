#include <bts/rpc/rpc_client.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/future.hpp>


namespace bts { namespace rpc { 

  namespace detail 
  {
    class rpc_client_impl
    {
    public:      
      fc::rpc::json_connection_ptr _json_connection;
      fc::future<void> _json_exec_loop_complete;

      void connect_to(const fc::ip::endpoint& remote_endpoint);

      bool login(const std::string& username, const std::string& password);
      bool walletpassphrase(const std::string& passphrase, const fc::microseconds& timeout);
      bts::blockchain::address getnewaddress(const std::string& account);
      bts::blockchain::transaction_id_type sendtoaddress(const bts::blockchain::address& address, uint64_t amount,
                                                         const std::string& comment, const std::string& comment_to);
      std::unordered_set<bts::wallet::receive_address> list_receive_addresses();
      bts::blockchain::asset getbalance(bts::blockchain::asset_type asset_type);
      bts::blockchain::signed_transaction get_transaction(bts::blockchain::transaction_id_type trascaction_id);
      bts::blockchain::signed_block_header getblock(uint32_t block_num);
      bool validateaddress(bts::blockchain::address address);
      bool rescan(uint32_t block_num);
      bool import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password);
      bool import_private_key(const fc::sha256& hash, const std::string& label);
      bool open_wallet(const std::string& wallet_username, const std::string& wallet_passphrase);
      bool createwallet(const std::string& wallet_username, const std::string& wallet_passphrase, const std::string& spending_passphrase);
      fc::optional<std::string> currentwallet();
      bool closewallet();
      uint32_t getconnectioncount();
      fc::variants getpeerinfo();
      void _set_advanced_node_parameters(const fc::variant_object& params);
      void addnode(const fc::ip::endpoint& node, const std::string& command);
      void stop();
    };

    void rpc_client_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      fc::tcp_socket_ptr socket = std::make_shared<fc::tcp_socket>();
      try 
      {
        socket->connect_to(remote_endpoint);
      }
      catch ( const fc::exception& e )
      {
        elog( "fatal: error opening RPC socket to endpoint ${endpoint}: ${e}", ("endpoint", remote_endpoint)("e", e.to_detail_string() ) );
        throw;
      }

      fc::buffered_istream_ptr buffered_istream = std::make_shared<fc::buffered_istream>(socket);
      fc::buffered_ostream_ptr buffered_ostream = std::make_shared<fc::buffered_ostream>(socket);

      _json_connection = std::make_shared<fc::rpc::json_connection>(std::move(buffered_istream), 
                                                                    std::move(buffered_ostream));
      _json_exec_loop_complete = fc::async([=](){ _json_connection->exec(); }, "json exec loop");
    }

    bool rpc_client_impl::login(const std::string& username, const std::string& password)
    {
      return _json_connection->call<bool>("login", username, password);
    }

    bool rpc_client_impl::walletpassphrase(const std::string& passphrase, const fc::microseconds& timeout)
    {
      uint32_t timeout_seconds = timeout.count() / fc::seconds(1).count();
      return _json_connection->call<bool>("walletpassphrase", passphrase, fc::variant(timeout_seconds));
    }

    bts::blockchain::address rpc_client_impl::getnewaddress(const std::string& account)
    {
      return _json_connection->call<bts::blockchain::address>("getnewaddress", account);
    }

    bts::blockchain::transaction_id_type rpc_client_impl::sendtoaddress(const bts::blockchain::address& address, uint64_t amount,
                                                                        const std::string& comment, const std::string& comment_to)
    {
      return _json_connection->call<bts::blockchain::transaction_id_type>("sendtoaddress", fc::variant((std::string)address), fc::variant(amount), fc::variant(comment), fc::variant(comment_to));
    }

    std::unordered_set<bts::wallet::receive_address> rpc_client_impl::list_receive_addresses()
    {
      return _json_connection->call<std::unordered_set<bts::wallet::receive_address> >("list_receive_addresses");
    }

    bts::blockchain::asset rpc_client_impl::getbalance(bts::blockchain::asset_type asset_type)
    {
      return _json_connection->call<bts::blockchain::asset>("getbalance", fc::variant(asset_type));
    }

    bts::blockchain::signed_transaction rpc_client_impl::get_transaction(bts::blockchain::transaction_id_type trascaction_id)
    {
      return _json_connection->call<bts::blockchain::signed_transaction>("get_transaction", fc::variant(trascaction_id));
    }

    bts::blockchain::signed_block_header rpc_client_impl::getblock(uint32_t block_num)
    {
      return _json_connection->call<bts::blockchain::signed_block_header>("getblock", fc::variant(block_num));
    }

    bool rpc_client_impl::validateaddress(bts::blockchain::address address)
    {
      return _json_connection->call<bool>("getblock", fc::variant(address));
    }

    bool rpc_client_impl::rescan(uint32_t block_num)
    {
      return _json_connection->call<bool>("rescan", fc::variant(block_num));
    }

    bool rpc_client_impl::import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password)
    {
      return _json_connection->call<bool>("import_bitcoin_wallet", wallet_filename.string(), password);
    }

    bool rpc_client_impl::import_private_key(const fc::sha256& hash, const std::string& label)
    {
      return _json_connection->call<bool>("import_private_key", (std::string)hash, label);
    }
    bool rpc_client_impl::open_wallet(const std::string& wallet_username, const std::string& wallet_passphrase)
    {
      return _json_connection->call<bool>("open_wallet", wallet_username, wallet_passphrase);
    }
    bool rpc_client_impl::createwallet(const std::string& wallet_username, const std::string& wallet_passphrase, const std::string& spending_passphrase)
    {
      return _json_connection->call<bool>("createwallet", wallet_username, wallet_passphrase, spending_passphrase);
    }
    fc::optional<std::string> rpc_client_impl::currentwallet()
    {
      fc::variant result = _json_connection->async_call("currentwallet").wait();
      return result.is_null() ? fc::optional<std::string>() : result.as_string();
    }
    bool rpc_client_impl::closewallet()
    {
      return _json_connection->call<bool>("closewallet");
    }
    uint32_t rpc_client_impl::getconnectioncount()
    {
      return _json_connection->call<uint32_t>("getconnectioncount");
    }
    fc::variants rpc_client_impl::getpeerinfo()
    {
      return _json_connection->async_call("getpeerinfo").wait().get_array();
    }
    void rpc_client_impl::_set_advanced_node_parameters(const fc::variant_object& params)
    {
      _json_connection->async_call("_set_advanced_node_parameters", fc::variant(params)).wait();
    }
    void rpc_client_impl::addnode(const fc::ip::endpoint& node, const std::string& command)
    {
      _json_connection->async_call("addnode", (std::string)node, command).wait();
    }
    void rpc_client_impl::stop()
    {
      _json_connection->async_call("stop").wait();
    }
  } // end namespace detail


  rpc_client::rpc_client() :
    my(new detail::rpc_client_impl)
  {
  }

  rpc_client::~rpc_client()
  {
  }

  void rpc_client::connect_to(const fc::ip::endpoint& remote_endpoint)
  {
    my->connect_to(remote_endpoint);
  }

  bool rpc_client::login(const std::string& username, const std::string& password)
  {
    return my->login(username, password);
  }
  
  bool rpc_client::walletpassphrase(const std::string& passphrase, const fc::microseconds& timeout)
  {
    return my->walletpassphrase(passphrase, timeout);
  }

  bts::blockchain::address rpc_client::getnewaddress(const std::string& account)
  {
    return my->getnewaddress(account);
  }

  bts::blockchain::transaction_id_type rpc_client::sendtoaddress(const bts::blockchain::address& address, uint64_t amount,
                                                                 const std::string& comment, const std::string& comment_to)
  {
    return my->sendtoaddress(address, amount, comment, comment_to);
  }

  std::unordered_set<bts::wallet::receive_address> rpc_client::list_receive_addresses()
  {
    return my->list_receive_addresses();
  }

  bts::blockchain::asset rpc_client::getbalance(bts::blockchain::asset_type asset_type)
  {
    return my->getbalance(asset_type);
  }

  bts::blockchain::signed_transaction rpc_client::get_transaction(bts::blockchain::transaction_id_type trascaction_id)
  {
    return my->get_transaction(trascaction_id);
  }

  bts::blockchain::signed_block_header rpc_client::getblock(uint32_t block_num)
  {
    return my->getblock(block_num);
  }

  bool rpc_client::validateaddress(bts::blockchain::address address)
  {
    return my->validateaddress(address);
  }

  bool rpc_client::rescan(uint32_t block_num /* = 0 */)
  {
    return my->rescan(block_num);
  }

  bool rpc_client::import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password)
  {
    return my->import_bitcoin_wallet(wallet_filename, password);
  }

  bool rpc_client::import_private_key(const fc::sha256& hash, const std::string& label)
  {
    return my->import_private_key(hash, label);
  }

  bool rpc_client::open_wallet(const std::string& wallet_username, const std::string& wallet_passphrase)
  {
    return my->open_wallet(wallet_username, wallet_passphrase);
  }
  bool rpc_client::createwallet(const std::string& wallet_username, const std::string& wallet_passphrase, const std::string& spending_passphrase)
  {
    return my->createwallet(wallet_username, wallet_passphrase, spending_passphrase);
  }
  fc::optional<std::string> rpc_client::currentwallet()
  {
    return my->currentwallet();
  }
  bool rpc_client::closewallet()
  {
    return my->closewallet();
  }
  uint32_t rpc_client::getconnectioncount()
  {
    return my->getconnectioncount();
  }
  fc::variants rpc_client::getpeerinfo()
  {
    return my->getpeerinfo();
  }
  void rpc_client::_set_advanced_node_parameters(const fc::variant_object& params)
  {
    my->_set_advanced_node_parameters(params);
  }
  void rpc_client::addnode(const fc::ip::endpoint& node, const std::string& command)
  {
    my->addnode(node, command);
  }
  void rpc_client::stop()
  {
    my->stop();
  }

} } // bts::rpc
