#include <bts/rpc/rpc_client.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/base58.hpp>
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
      bool wallet_unlock(const fc::microseconds& timeout, const std::string& passphrase);
      bts::blockchain::extended_address wallet_create_receive_account(const std::string& account_name);
      void wallet_create_sending_account(const std::string& account_name, const bts::blockchain::extended_address& account_key);
      std::vector<std::string> wallet_list_receive_accounts(int32_t start = 0, uint32_t count = -1);
      bts::wallet::invoice_summary wallet_transfer(int64_t amount, const std::string& sending_account_name,
                                                   const std::string& invoice_memo = "",
                                                   const std::string& from_account = "*",
                                                   uint32_t asset_id = 0);
      std::unordered_map<blockchain::address,std::string> list_receive_addresses()const;
      bts::blockchain::asset wallet_get_balance(const std::string& account_name = "*", int minconf = 0, int asset = 0);
      bts::blockchain::signed_transaction get_transaction(bts::blockchain::transaction_id_type trascaction_id);
      bts::blockchain::signed_block_header getblock(uint32_t block_num);
      bool validateaddress(bts::blockchain::address address);
      bool wallet_rescan_blockchain(uint32_t block_num);
      bool import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password);
      bool wallet_import_private_key(const fc::ecc::private_key& key, const std::string& account_name = "default", bool rescan_blockchain = false);
      bool wallet_open(const std::string& wallet_name, const std::string& wallet_passphrase);
      bool wallet_create(const std::string& wallet_name, const std::string& wallet_passphrase);
      fc::optional<std::string> wallet_get_name();
      bool wallet_close();
      void wallet_lock();
      uint32_t network_get_connection_count();
      fc::variants network_get_peer_info();
      fc::variant_object get_info();
      void _set_advanced_node_parameters(const fc::variant_object& params);
      bts::net::message_propagation_data _get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id);
      bts::net::message_propagation_data _get_block_propagation_data(const bts::blockchain::block_id_type& block_id);
      void _set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers);

      void network_add_node(const fc::ip::endpoint& node, const std::string& command);
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

    bool rpc_client_impl::wallet_unlock(const fc::microseconds& timeout, const std::string& passphrase)
    {
      uint32_t timeout_seconds = (uint32_t)(timeout.count() / fc::seconds(1).count());
      return _json_connection->call<bool>("wallet_unlock", fc::variant(timeout_seconds), passphrase);
    }

    bts::blockchain::extended_address rpc_client_impl::wallet_create_receive_account(const std::string& account_name)
    {
      return _json_connection->call<bts::blockchain::extended_address>("wallet_create_receive_account", account_name);
    }

    void rpc_client_impl::wallet_create_sending_account(const std::string& account_name, const bts::blockchain::extended_address& account_key)
    {
      _json_connection->async_call("wallet_create_sending_account", account_name, fc::variant(account_key)).wait();
    }

    std::vector<std::string> rpc_client_impl::wallet_list_receive_accounts(int32_t start, uint32_t count)
    {
      return _json_connection->call<std::vector<std::string> >("wallet_list_receive_accounts", fc::variant(start), fc::variant(count));
    }

    bts::wallet::invoice_summary rpc_client_impl::wallet_transfer(int64_t amount, const std::string& sending_account_name,
                                                                  const std::string& invoice_memo,
                                                                  const std::string& from_account,
                                                                  uint32_t asset_id)
    {
      fc::mutable_variant_object named_params;
      named_params["invoice_memo"] = invoice_memo;
      named_params["from_account"] = from_account;
      named_params["asset_id"] = asset_id;
      return _json_connection->call<bts::wallet::invoice_summary>("wallet_transfer", fc::variant(amount), fc::variant(sending_account_name), named_params);
    }

    std::unordered_map<blockchain::address,std::string> rpc_client_impl::list_receive_addresses()const
    {
      return _json_connection->call<std::unordered_map<blockchain::address,std::string> >("list_receive_addresses");
    }

    bts::blockchain::asset rpc_client_impl::wallet_get_balance(const std::string& account_name, int minconf, int asset)
    {
      return _json_connection->call<bts::blockchain::asset>("wallet_get_balance", account_name, minconf, asset);
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

    bool rpc_client_impl::wallet_rescan_blockchain(uint32_t block_num)
    {
      return _json_connection->call<bool>("wallet_rescan_blockchain", fc::variant(block_num));
    }

    bool rpc_client_impl::import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password)
    {
      return _json_connection->call<bool>("import_bitcoin_wallet", wallet_filename.string(), password);
    }

    bool rpc_client_impl::wallet_import_private_key(const fc::ecc::private_key& key, const std::string& account_name, bool rescan_blockchain)
    {
      return _json_connection->call<bool>("wallet_import_private_key", bts::utilities::key_to_wif(key), account_name, rescan_blockchain);
    }
    bool rpc_client_impl::wallet_open(const std::string& wallet_name, const std::string& wallet_passphrase)
    {
      return _json_connection->call<bool>("wallet_open", wallet_name, wallet_passphrase);
    }
    bool rpc_client_impl::wallet_create(const std::string& wallet_name, const std::string& wallet_passphrase)
    {
      return _json_connection->call<bool>("wallet_create", wallet_name, wallet_passphrase);
    }
    fc::optional<std::string> rpc_client_impl::wallet_get_name()
    {
      fc::variant result = _json_connection->async_call("wallet_get_name").wait();
      return result.is_null() ? fc::optional<std::string>() : result.as_string();
    }
    bool rpc_client_impl::wallet_close()
    {
      return _json_connection->call<bool>("wallet_close");
    }
    void rpc_client_impl::wallet_lock()
    {
      _json_connection->async_call("wallet_lock").wait();
    }
    uint32_t rpc_client_impl::network_get_connection_count()
    {
      return _json_connection->call<uint32_t>("network_get_connection_count");
    }
    fc::variants rpc_client_impl::network_get_peer_info()
    {
      return _json_connection->async_call("network_get_peer_info").wait().get_array();
    }
    fc::variant_object rpc_client_impl::get_info()
    {
      return _json_connection->async_call("get_info").wait().get_object();
    }
    void rpc_client_impl::_set_advanced_node_parameters(const fc::variant_object& params)
    {
      _json_connection->async_call("_set_advanced_node_parameters", fc::variant(params)).wait();
    }
    bts::net::message_propagation_data rpc_client_impl::_get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
    {
      return _json_connection->call<bts::net::message_propagation_data>("_get_transaction_propagation_data", fc::variant(transaction_id));
    }
    bts::net::message_propagation_data rpc_client_impl::_get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
    {
      return _json_connection->call<bts::net::message_propagation_data>("_get_block_propagation_data", fc::variant(block_id));
    }
    void rpc_client_impl::_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers)
    {
      _json_connection->async_call("_set_allowed_peers", fc::variant(allowed_peers)).wait();
    }
    void rpc_client_impl::network_add_node(const fc::ip::endpoint& node, const std::string& command)
    {
      _json_connection->async_call("network_add_node", (std::string)node, command).wait();
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
  
  bool rpc_client::wallet_unlock(const fc::microseconds& timeout, const std::string& passphrase)
  {
    return my->wallet_unlock(timeout, passphrase);
  }

  bts::blockchain::extended_address rpc_client::wallet_create_receive_account(const std::string& account_name)
  {
    return my->wallet_create_receive_account(account_name);
  }

  void rpc_client::wallet_create_sending_account(const std::string& account_name, const bts::blockchain::extended_address& account_key)
  {
    my->wallet_create_sending_account(account_name, account_key);
  }

  std::vector<std::string> rpc_client::wallet_list_receive_accounts(int32_t start, uint32_t count)
  {
    return my->wallet_list_receive_accounts(start, count);
  }

  bts::wallet::invoice_summary rpc_client::wallet_transfer(int64_t amount, const std::string& sending_account_name,
                                                           const std::string& invoice_memo,
                                                           const std::string& from_account,
                                                           uint32_t asset_id)
  {
    return my->wallet_transfer(amount, sending_account_name, invoice_memo, from_account, asset_id);
  }

  std::unordered_map<blockchain::address,std::string> rpc_client::list_receive_addresses()const
  {
    return my->list_receive_addresses();
  }

  bts::blockchain::asset rpc_client::wallet_get_balance(const std::string& account_name, int minconf, int asset)
  {
    return my->wallet_get_balance(account_name, minconf, asset);
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

  bool rpc_client::wallet_rescan_blockchain(uint32_t block_num /* = 0 */)
  {
    return my->wallet_rescan_blockchain(block_num);
  }

  bool rpc_client::import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password)
  {
    return my->import_bitcoin_wallet(wallet_filename, password);
  }

  bool rpc_client::wallet_import_private_key(const fc::ecc::private_key& key, 
                                             const std::string& account_name, 
                                             bool rescan_blockchain)
  {
    return my->wallet_import_private_key(key, account_name, rescan_blockchain);
  }
  bool rpc_client::wallet_open(const std::string& wallet_name, const std::string& wallet_passphrase)
  {
    return my->wallet_open(wallet_name, wallet_passphrase);
  }
  bool rpc_client::wallet_create(const std::string& wallet_name, const std::string& wallet_passphrase)
  {
    return my->wallet_create(wallet_name, wallet_passphrase);
  }
  fc::optional<std::string> rpc_client::wallet_get_name()
  {
    return my->wallet_get_name();
  }
  bool rpc_client::wallet_close()
  {
    return my->wallet_close();
  }
  void rpc_client::wallet_lock()
  {
    my->wallet_lock();
  }
  uint32_t rpc_client::network_get_connection_count()
  {
    return my->network_get_connection_count();
  }
  fc::variants rpc_client::network_get_peer_info()
  {
    return my->network_get_peer_info();
  }
  fc::variant_object rpc_client::get_info()
  {
    return my->get_info();
  }
  void rpc_client::_set_advanced_node_parameters(const fc::variant_object& params)
  {
    my->_set_advanced_node_parameters(params);
  }
  bts::net::message_propagation_data rpc_client::_get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
  {
    return my->_get_transaction_propagation_data(transaction_id);
  }
  bts::net::message_propagation_data rpc_client::_get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
  {
    return my->_get_block_propagation_data(block_id);
  }
  void rpc_client::_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers)
  {
    return my->_set_allowed_peers(allowed_peers);
  }
  void rpc_client::network_add_node(const fc::ip::endpoint& node, const std::string& command)
  {
    my->network_add_node(node, command);
  }
  void rpc_client::stop()
  {
    my->stop();
  }

} } // bts::rpc
