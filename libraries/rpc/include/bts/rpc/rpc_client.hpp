#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/net/node.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>

namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }


  /**
  *  @class rpc_client
  *  @brief provides a C++ interface to a remote BTS client over JSON-RPC
  */
  class rpc_client
  {
  public:
    rpc_client();
    ~rpc_client();

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
    bts::blockchain::transaction_id_type sendtoaddress(const bts::blockchain::address& address, uint64_t amount,
                                                       const std::string& comment = "", const std::string& comment_to = "");
    std::unordered_map<blockchain::address,std::string> list_receive_addresses()const;
    bts::blockchain::asset wallet_get_balance(const std::string& account_name = "*", int minconf = 0, int asset = 0);
    bts::blockchain::signed_transaction get_transaction(bts::blockchain::transaction_id_type trascaction_id);
    bts::blockchain::signed_block_header getblock(uint32_t block_num);
    bool validateaddress(bts::blockchain::address address);
    bool wallet_rescan_blockchain(uint32_t block_num = 0);
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
  private:
    std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
