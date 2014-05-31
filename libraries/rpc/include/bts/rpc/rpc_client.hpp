#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/net/node.hpp>

#include <bts/rpc/rpc_client_api.hpp>
#include <bts/rpc_stubs/common_api_client.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>

namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }


  /**
  *  @class rpc_client
  *  @brief provides a C++ interface to a remote BTS client over JSON-RPC
  */
  class rpc_client : public rpc_client_api,
                     public bts::rpc_stubs::common_api_client
  {
  public:
    rpc_client();
    virtual ~rpc_client();

    void connect_to(const fc::ip::endpoint& remote_endpoint);

    bool login(const std::string& username, const std::string& password);
    virtual fc::rpc::json_connection_ptr get_json_connection() const override;

         //-------------------------------------------------- JSON-RPC Method Implementations
         //TODO? help()

                     signed_transaction wallet_asset_create(const std::string& symbol,
                                                            const std::string& asset_name,
                                                            const std::string& description,
                                                            const fc::variant& data,
                                                            const std::string& issuer_name,
                                                            share_type maximum_share_supply,
                                                            generate_transaction_flag flag = sign_and_broadcast) override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                     signed_transaction wallet_asset_issue(share_type amount,
                                                          const std::string& symbol,
                                                          const std::string& to_account_name,
                                                          generate_transaction_flag flag = sign_and_broadcast) override { FC_ASSERT(false, "NOT IMPLEMENTED"); };

                     /**
                     *  Submit and vote on proposals
                     */
                     signed_transaction wallet_submit_proposal(const std::string& name,
                                                                 const std::string& subject,
                                                                 const std::string& body,
                                                                 const std::string& proposal_type,
                                                                 const fc::variant& json_data,
                                                                 generate_transaction_flag = sign_and_broadcast) override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                     signed_transaction wallet_vote_proposal( const std::string& name,
                                                               proposal_id_type proposal_id,
                                                               uint8_t vote,
                                                               generate_transaction_flag = sign_and_broadcast) override { FC_ASSERT(false, "NOT IMPLEMENTED"); };



                  wallet_account_record wallet_get_account(const std::string& account_name) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                                balances wallet_get_balance(const std::string& symbol = BTS_ADDRESS_PREFIX, const std::string& account_name = "*") const override;

         std::vector<wallet_transaction_record> wallet_get_transaction_history(unsigned count) const override;
         std::vector<pretty_transaction> wallet_get_transaction_history_summary(unsigned count) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                           oaccount_record blockchain_get_account_record(const std::string& name) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                           oaccount_record blockchain_get_account_record_by_id(name_id_type name_id) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                          oasset_record blockchain_get_asset_record(const std::string& symbol) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                          oasset_record blockchain_get_asset_record_by_id(asset_id_type asset_id) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };


                 
         void                               wallet_set_delegate_trust_status(const std::string& delegate_name, int32_t user_trust_level) override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
         //bts::wallet::delegate_trust_status wallet_get_delegate_trust_status(const std::string& delegate_name) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
         //std::map<std::string, bts::wallet::delegate_trust_status> wallet_list_delegate_trust_status() const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };

                        osigned_transaction blockchain_get_transaction(const transaction_id_type& transaction_id) const override { FC_ASSERT(false, "NOT IMPLEMENTED"); };
                                 full_block blockchain_get_block(const block_id_type& block_id) const override;
                                 full_block blockchain_get_block_by_number(uint32_t block_number) const override;

                       void wallet_rescan_blockchain(uint32_t starting_block_number = 0) override;
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
