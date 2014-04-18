#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>

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
    bts::blockchain::transaction_id_type transfer(const bts::blockchain::asset& amount, const bts::blockchain::address& address);
    bts::blockchain::asset getbalance(bts::blockchain::asset_type asset_type);
    bts::blockchain::signed_transaction get_transaction(bts::blockchain::transaction_id_type trascaction_id);
    bts::blockchain::signed_block_header getblock(uint32_t block_num);
    bool validateaddress(bts::blockchain::address address);
    bool import_bitcoin_wallet(const fc::path& wallet_filename, const std::string& password);

  private:
    std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
