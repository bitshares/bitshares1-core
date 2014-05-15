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
    bool walletpassphrase(const std::string& passphrase, const fc::microseconds& timeout);
    bts::blockchain::address getnewaddress(const std::string& account = "");
    bts::blockchain::transaction_id_type sendtoaddress(const bts::blockchain::address& address, uint64_t amount,
                                                       const std::string& comment = "", const std::string& comment_to = "");
    std::unordered_map<blockchain::address,std::string> list_receive_addresses()const;
    bts::blockchain::asset getbalance(bts::blockchain::asset_id_type asset_type);
    bts::blockchain::signed_transaction get_transaction(bts::blockchain::transaction_id_type trascaction_id);
    bts::blockchain::signed_block_header getblock(uint32_t block_num);
    bool validateaddress(bts::blockchain::address address);
    bool rescan(uint32_t block_num = 0);
    bool import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password);
    bool import_private_key(const fc::sha256& hash, const std::string& label);
    bool wallet_open(const std::string& wallet_name, const std::string& wallet_passphrase);
    bool wallet_create(const std::string& wallet_name, const std::string& wallet_passphrase);
    fc::optional<std::string> currentwallet();
    bool closewallet();
    uint32_t getconnectioncount();
    fc::variants getpeerinfo();
    fc::variant_object getinfo();
    void _set_advanced_node_parameters(const fc::variant_object& params);
    bts::net::message_propagation_data _get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id);
    bts::net::message_propagation_data _get_block_propagation_data(const bts::blockchain::block_id_type& block_id);

    void addnode(const fc::ip::endpoint& node, const std::string& command);
    void stop();
  private:
    std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
