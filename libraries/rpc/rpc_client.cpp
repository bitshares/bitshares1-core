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

#include <bts/rpc_stubs/common_api_client.hpp>

namespace bts { namespace rpc { 

  namespace detail 
  {
    class rpc_client_impl : public bts::rpc_stubs::common_api_client
    {
    public:
      fc::rpc::json_connection_ptr _json_connection;
      fc::future<void> _json_exec_loop_complete;

      virtual fc::rpc::json_connection_ptr get_json_connection() const override { return _json_connection; }

      void connect_to(const fc::ip::endpoint& remote_endpoint);

      bool login(const std::string& username, const std::string& password);
      std::map<std::string, extended_address> wallet_list_receive_accounts(uint32_t start = 0, uint32_t count = -1);
      /*
      bts::wallet::invoice_summary wallet_transfer(int64_t amount, 
                                                   const std::string& to_account_name,                                                   
                                                   const std::string& asset_symbol,
                                                   const std::string& from_account = "*",
                                                   const std::string& invoice_memo = "",
                                                   rpc_client_api::generate_transaction_flag flag = rpc_client_api::sign_and_broadcast);
                                                   */
      full_block blockchain_get_block(const block_id_type& block_id);
      full_block blockchain_get_block_by_number(uint32_t block_number);
      bool validate_address(bts::blockchain::address address);
      void wallet_import_bitcoin(const fc::path& wallet_filename, const std::string& password, const std::string& account_name );
      void wallet_import_private_key(const std::string& wif_key_to_import, const std::string& account_name = "default", bool wallet_rescan_blockchain = false);
      bts::net::message_propagation_data network_get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id);
      bts::net::message_propagation_data network_get_block_propagation_data(const bts::blockchain::block_id_type& block_id);
      void network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers);

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

    std::map<std::string, extended_address> rpc_client_impl::wallet_list_receive_accounts(uint32_t start, uint32_t count)
    {
      FC_ASSERT(false)
      //return _json_connection->call<std::vector<std::string> >("wallet_list_receive_accounts", fc::variant(start), fc::variant(count));
    }

    /*
    bts::wallet::invoice_summary rpc_client_impl::wallet_transfer(int64_t amount, 
                                                                  const std::string& to_account_name,
                                                                  const std::string& asset_symbol,
                                                                  const std::string& from_account_name,
                                                                  const std::string& invoice_memo,
                                                                  rpc_client_api::generate_transaction_flag flag)
    {
      fc::mutable_variant_object named_params;
      named_params["asset_symbol"] = asset_symbol;
      named_params["from_account"] = from_account_name;
      named_params["invoice_memo"] = invoice_memo;
      return _json_connection->call<bts::wallet::invoice_summary>("wallet_transfer", fc::variant(amount), fc::variant(to_account_name), named_params);
    }
    */


    full_block rpc_client_impl::blockchain_get_block(const block_id_type&  block_id)
    {
      return _json_connection->call<full_block>("blockchain_get_block", fc::variant(block_id));
    }

    full_block rpc_client_impl::blockchain_get_block_by_number(uint32_t block_number)
    {
      return _json_connection->call<full_block>("blockchain_get_block_by_number", fc::variant(block_number));
    }

    bool rpc_client_impl::validate_address(bts::blockchain::address address)
    {
      return _json_connection->call<bool>("blockchain_get_block", fc::variant(address));
    }


    void rpc_client_impl::wallet_import_bitcoin(const fc::path& wallet_filename, const std::string& password, const std::string& account_name )
    {
      _json_connection->async_call("wallet_import_bitcoin", wallet_filename.string(), password, account_name ).wait();
    }

    void rpc_client_impl::wallet_import_private_key(const std::string& wif_key_to_import, const std::string& account_name, bool rescan_blockchain)
    {
      _json_connection->async_call("wallet_import_private_key", wif_key_to_import, account_name, rescan_blockchain).wait();
    }
    bts::net::message_propagation_data rpc_client_impl::network_get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
    {
      return _json_connection->call<bts::net::message_propagation_data>("network_get_transaction_propagation_data", fc::variant(transaction_id));
    }
    bts::net::message_propagation_data rpc_client_impl::network_get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
    {
      return _json_connection->call<bts::net::message_propagation_data>("network_get_block_propagation_data", fc::variant(block_id));
    }
    void rpc_client_impl::network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers)
    {
      _json_connection->async_call("network_set_allowed_peers", fc::variant(allowed_peers)).wait();
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
  
  fc::rpc::json_connection_ptr rpc_client::get_json_connection() const
  {
    return my->_json_connection;
  }

  /*
  std::map<std::string, extended_address> rpc_client::wallet_list_receive_accounts(uint32_t start, uint32_t count)
  {
    FC_ASSERT(false)
    //return my->wallet_list_receive_accounts(start, count);
  }
  */

  balances rpc_client::wallet_get_balance(const std::string& asset_symbol, 
                                       const std::string& account_name) const
  {
    return my->wallet_get_balance(asset_symbol, account_name);
  }


  full_block rpc_client::blockchain_get_block(const block_id_type& block_id) const
  {
    return my->blockchain_get_block(block_id);
  }

  full_block rpc_client::blockchain_get_block_by_number(uint32_t block_number) const
  {
    return my->blockchain_get_block_by_number(block_number);
  }
  /*
  bool rpc_client::validate_address(bts::blockchain::address address)
  {
    return my->validate_address(address);
  }
  */

  void rpc_client::wallet_import_bitcoin(const fc::path& wallet_filename, const std::string& password, const std::string& account_name)
  {
    my->wallet_import_bitcoin(wallet_filename, password, account_name );
  }

  void rpc_client::wallet_import_private_key(const std::string& wif_key_to_import,
                                             const std::string& account_name, 
                                             bool wallet_rescan_blockchain)
  {
    my->wallet_import_private_key(wif_key_to_import, account_name, wallet_rescan_blockchain);
  }

  void rpc_client::network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers)
  {
    return my->network_set_allowed_peers(allowed_peers);
  }
  void rpc_client::stop()
  {
    my->stop();
  }

} } // bts::rpc
