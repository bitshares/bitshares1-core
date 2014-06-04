#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/net/node.hpp>

#include <bts/rpc/rpc_client_api.hpp>
#include <bts/rpc_stubs/common_api_rpc_client.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>

namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }


  /**
  *  @class rpc_client
  *  @brief provides a C++ interface to a remote BTS client over JSON-RPC
  */
  class rpc_client : public rpc_client_api,
                     public bts::rpc_stubs::common_api_rpc_client
  {
  public:
    rpc_client();
    virtual ~rpc_client();

    void connect_to(const fc::ip::endpoint& remote_endpoint);

    bool login(const std::string& username, const std::string& password);
    virtual fc::rpc::json_connection_ptr get_json_connection() const override;

         //-------------------------------------------------- JSON-RPC Method Implementations
         //TODO? help()

                     /**
                     *  Submit and vote on proposals
                     */


                  wallet_account_record wallet_get_account(const std::string& account_name) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                                balances wallet_get_balance(const std::string& symbol = BTS_ADDRESS_PREFIX, const std::string& account_name = "*") const override;


                        osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                       void wallet_rescan_blockchain_state() override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                       void wallet_import_bitcoin(const fc::path& filename,const std::string& passphrase, const std::string& account_name) override;
                       void wallet_import_private_key(const std::string& wif_key_to_import, 
                                                      const std::string& account_name,
                                                      bool wallet_rescan_blockchain = false) override;

     //std::vector<account_record> blockchain_list_registered_accounts(const std::string& first, uint32_t count) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
    std::vector<asset_record> blockchain_get_assets(const std::string& first_symbol, uint32_t count) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
     std::vector<account_record> blockchain_get_delegates(uint32_t first, uint32_t count) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };

                         void stop() override;

                      void network_set_allowed_peers(const std::vector<bts::net::node_id_t>& allowed_peers);


  private:
    std::unique_ptr<detail::rpc_client_impl> my;
  };
  typedef std::shared_ptr<rpc_client> rpc_client_ptr;
} } // bts::rpc
