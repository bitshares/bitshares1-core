#include <algorithm>

#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/node.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

#include <bts/rpc/rpc_client.hpp>
#include <bts/api/common_api.hpp>

#include <bts/rpc/rpc_server.hpp>

#include <bts/utilities/git_revision.hpp>
#include <fc/git_revision.hpp>

#include <iostream>

namespace bts { namespace client {

    namespace detail
    {
       class client_impl : public bts::net::node_delegate
       {
          public:
            client_impl(bts::client::client* self) :
              _self(self),
              _chain_db(std::make_shared<chain_database>()),
              _rpc_server(std::make_shared<rpc_server>(self))
            {
            }

            virtual ~client_impl()override {}

            void delegate_loop();
            signed_transactions get_pending_transactions() const;

            /* Implement chain_client_impl */
            // @{
            virtual void on_new_block(const full_block& block);
            virtual void on_new_transaction(const signed_transaction& trx);
            /// @}

            /* Implement node_delegate */
            // @{
            virtual bool has_item(const bts::net::item_id& id) override;
            virtual void handle_message(const bts::net::message&) override;
            virtual vector<bts::net::item_hash_t> get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit = 2000) override;
            virtual bts::net::message get_item(const bts::net::item_id& id) override;
            virtual fc::sha256 get_chain_id() const override
            { 
                FC_ASSERT( _chain_db != nullptr );
                return _chain_db->chain_id(); 
            }
            virtual vector<bts::net::item_hash_t> get_blockchain_synopsis() override;
            virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
            virtual void connection_count_changed(uint32_t c) override;
            /// @}
            bts::client::client*                                        _self;
            fc::time_point                                              _last_block;
            fc::path                                                    _data_dir;

            bts::rpc::rpc_server_ptr                                    _rpc_server;
            bts::net::node_ptr                                          _p2p_node;
            chain_database_ptr                                          _chain_db;
            unordered_map<transaction_id_type, signed_transaction> _pending_trxs;
            wallet_ptr                                                  _wallet;
            fc::future<void>                                            _delegate_loop_complete;
       };

       void client_impl::delegate_loop()
       {
          fc::usleep( fc::seconds( 10 ) );
         _last_block = _chain_db->get_head_block().timestamp;
         while( !_delegate_loop_complete.canceled() )
         {
            auto now = fc::time_point_sec(fc::time_point::now());
            auto next_block_time = _wallet->next_block_production_time();
            ilog( "next block time: ${b}  interval: ${i} seconds  now: ${n}",
                  ("b",next_block_time)("i",BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC)("n",now) );
            if( next_block_time < (now + -1) ||
                (next_block_time - now) > fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) )
            {
               fc::usleep( fc::seconds(2) );
               continue;
            }
            else
            {
               if( _wallet->is_unlocked() )
               {
                  ilog( "producing block in: ${b}", ("b",(next_block_time-now).count()/1000000.0) );
                  try {
                     fc::usleep( (next_block_time - now) );
                     full_block next_block = _chain_db->generate_block( next_block_time );
                     _wallet->sign_block( next_block );

                     on_new_block(next_block);
                     _p2p_node->broadcast(block_message( next_block ));
                  }
                  catch ( const fc::exception& e )
                  {
                     wlog( "${e}", ("e",e.to_detail_string() ) );
                  }
               }
               else
               {
                  elog( "unable to produce block because wallet is locked" );
               }
            }
            fc::usleep( fc::seconds(1) );
         }
       } // delegate_loop

       signed_transactions client_impl::get_pending_transactions() const
       {
         signed_transactions trxs;
         trxs.reserve(_pending_trxs.size());
         for (auto trx : _pending_trxs)
         {
           trxs.push_back(trx.second);
         }
         return trxs;
       }

       ///////////////////////////////////////////////////////
       // Implement chain_client_delegate                   //
       ///////////////////////////////////////////////////////
       void client_impl::on_new_block(const full_block& block)
       {
         try
         {
           ilog("Received a new block from the server, current head block is ${num}, block is ${block}",
                ("num", _chain_db->get_head_block_num())("block", block));

           _chain_db->push_block(block);
         }
         catch (fc::exception& e)
         {
           wlog("Error pushing block ${block}: ${error}", ("block", block)("error", e.to_string()));
           throw;
         }
       }

       void client_impl::on_new_transaction(const signed_transaction& trx)
       {
         _chain_db->store_pending_transaction(trx); // throws exception if invalid trx.
       }

       ///////////////////////////////////////////////////////
       // Implement node_delegate                           //
       ///////////////////////////////////////////////////////
       bool client_impl::has_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
           return _chain_db->is_known_block( id.item_hash );
         }

         if (id.item_type == trx_message_type)
         {
           return _chain_db->is_known_transaction( id.item_hash );
         }
         return false;
       }

       void client_impl::handle_message(const bts::net::message& message_to_handle)
       {
         switch (message_to_handle.msg_type)
         {
            case block_message_type:
              {
                block_message block_message_to_handle(message_to_handle.as<block_message>());
                ilog("CLIENT: just received block ${id}", ("id", block_message_to_handle.block.id()));
                on_new_block(block_message_to_handle.block);
                break;
              }
            case trx_message_type:
              {
                trx_message trx_message_to_handle(message_to_handle.as<trx_message>());
                ilog("CLIENT: just received transaction ${id}", ("id", trx_message_to_handle.trx.id()));
                on_new_transaction(trx_message_to_handle.trx);
                break;
              }
         }
       }

       /**
        *  Get the hash of all blocks after from_id
        */
       vector<bts::net::item_hash_t> client_impl::get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit /* = 2000 */)
       {
         FC_ASSERT(from_id.item_type == bts::client::block_message_type);
         ilog("head_block is ${head_block_num}", ("head_block_num", _chain_db->get_head_block_num()));

         uint32_t last_seen_block_num;
         try
         {
           last_seen_block_num = _chain_db->get_block_num(from_id.item_hash);
         }
         catch (fc::key_not_found_exception&)
         {
           if (from_id.item_hash == bts::net::item_hash_t())
             last_seen_block_num = 0;
           else
           {
             remaining_item_count = 0;
             return vector<bts::net::item_hash_t>();
           }
         }
         remaining_item_count = _chain_db->get_head_block_num() - last_seen_block_num;
         uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
         vector<bts::net::item_hash_t> hashes_to_return;
         hashes_to_return.reserve(items_to_get_this_iteration);
         for (uint32_t i = 0; i < items_to_get_this_iteration; ++i)
         {
           ++last_seen_block_num;
           signed_block_header header;
           try
           {
             header = _chain_db->get_block(last_seen_block_num);
           }
           catch (fc::key_not_found_exception&)
           {
             ilog( "attempting to fetch last_seen ${i}", ("i",last_seen_block_num) );
             assert( !"I assume this can never happen");
           }
           hashes_to_return.push_back(header.id());
         }
         remaining_item_count -= items_to_get_this_iteration;
         return hashes_to_return;
       }

      vector<bts::net::item_hash_t> client_impl::get_blockchain_synopsis()
      {
        vector<bts::net::item_hash_t> synopsis;
        uint32_t high_block_num = _chain_db->get_head_block_num();
        uint32_t low_block_num = 1;
        do
        {
          synopsis.push_back(_chain_db->get_block(low_block_num).id());
          low_block_num += ((high_block_num - low_block_num + 2) / 2);
        }
        while (low_block_num <= high_block_num);

        return synopsis;
      }

       bts::net::message client_impl::get_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
        //   uint32_t block_number = _chain_db->get_block_num(id.item_hash);
           bts::client::block_message block_message_to_send(_chain_db->get_block(id.item_hash));
           FC_ASSERT(id.item_hash == block_message_to_send.block_id); //.id());
        //   block_message_to_send.signature = block_message_to_send.block.delegate_signature;
           return block_message_to_send;
         }

         if (id.item_type == trx_message_type)
         {
           trx_message trx_message_to_send;
           auto iter = _pending_trxs.find(id.item_hash);
           if (iter != _pending_trxs.end())
             trx_message_to_send.trx = iter->second;
         }

         FC_THROW_EXCEPTION(key_not_found_exception, "I don't have the item you're looking for");
       }

       void client_impl::sync_status(uint32_t item_type, uint32_t item_count)
       {
       }

       void client_impl::connection_count_changed(uint32_t c)
       {
       }

    }

    client::client()
    :my( new detail::client_impl(this))
    {
    }

    client::client(bts::net::simulated_network_ptr network_to_connect_to)
    : my( new detail::client_impl(this) )
    {
      network_to_connect_to->add_node_delegate(my.get());
      my->_p2p_node = network_to_connect_to;
    }

    void client::open( const path& data_dir, const path& genesis_dat )
    { try {
        my->_chain_db->open( data_dir / "chain", genesis_dat );
        my->_wallet = std::make_shared<bts::wallet::wallet>( my->_chain_db );
        my->_wallet->set_data_directory( data_dir / "wallets" );

        //if we are using a simulated network, _p2p_node will already be set by client's constructor
        if (!my->_p2p_node)
        {
          my->_p2p_node = std::make_shared<bts::net::node>();
        }
        my->_p2p_node->set_node_delegate(my.get());
    } FC_RETHROW_EXCEPTIONS( warn, "", ("data_dir",data_dir)
                             ("genesis_dat", fc::absolute(genesis_dat)) ) }

    client::~client()
    {
       try {
          if( my->_delegate_loop_complete.valid() )
          {
             my->_delegate_loop_complete.cancel();
             ilog( "waiting for delegate loop to complete" );
             my->_delegate_loop_complete.wait();
          }
       }
       catch ( const fc::canceled_exception& ) {}
       catch ( const fc::exception& e )
       {
          wlog( "${e}", ("e",e.to_detail_string() ) );
       }
    }

    wallet_ptr client::get_wallet()const { return my->_wallet; }
    chain_database_ptr client::get_chain()const { return my->_chain_db; }
    bts::rpc::rpc_server_ptr client::get_rpc_server() const { return my->_rpc_server; }
    bts::net::node_ptr client::get_node()const { return my->_p2p_node; }
    signed_transactions client::get_pending_transactions()const { return my->get_pending_transactions(); }

    bts::blockchain::transaction_id_type client::network_broadcast_transaction(const bts::blockchain::signed_transaction& transaction_to_broadcast)
    {
      ilog("broadcasting transaction with id ${id} : ${transaction}", 
           ("id", transaction_to_broadcast.id())("transaction", transaction_to_broadcast));

      // p2p doesn't send messages back to the originator
      my->on_new_transaction(transaction_to_broadcast);
      my->_p2p_node->broadcast(trx_message(transaction_to_broadcast));
      return transaction_to_broadcast.id();
    }

    //JSON-RPC Method Implementations START
    bts::blockchain::block_id_type client::blockchain_get_blockhash(uint32_t block_number) const
    {
      return get_chain()->get_block(block_number).id();
    }

    uint32_t client::blockchain_get_blockcount() const
    {
      return get_chain()->get_head_block_num();
    }

    void client::wallet_open_file(const fc::path& wallet_filename)
    {
      get_wallet()->open_file( wallet_filename );
    }

    void client::wallet_open(const string& wallet_name)
    {
      get_wallet()->open(wallet_name);
    }

    void client::wallet_create(const string& wallet_name, const std::string& password)
    {
      get_wallet()->create(wallet_name,password);
    }

    fc::optional<string> client::wallet_get_name() const
    {
      return get_wallet()->is_open() ? get_wallet()->get_wallet_name() : fc::optional<string>();
    }

    void client::wallet_close()
    {
      get_wallet()->close();
    }

    void client::wallet_export_to_json(const fc::path& path) const
    {
      get_wallet()->export_to_json(path);
    }

    void client::wallet_create_from_json(const fc::path& path, const string& name)
    {
      get_wallet()->create_from_json(path,name);
    }

    void client::wallet_lock()
    {
      get_wallet()->lock();
    }

    void client::wallet_unlock(const fc::microseconds& timeout, const string& password)
    {
      get_wallet()->unlock(password,timeout);
    }

    void client::wallet_change_passphrase(const string& new_password)
    {
      get_wallet()->change_passphrase(new_password);
    }


    vector<signed_transaction> client::wallet_transfer(int64_t amount_to_transfer, 
                                                       const string& asset_symbol, 
                                                       const string& from_account_name, 
                                                       const string& to_account_name, 
                                                       const string& memo_message)
    { try {
         auto trxs = get_wallet()->transfer( amount_to_transfer, asset_symbol, 
                                             from_account_name, to_account_name, 
                                             memo_message, true );
         for( auto trx : trxs )
         {
            network_broadcast_transaction( trx );
         }

         return trxs;
    } FC_RETHROW_EXCEPTIONS( warn, "" ) }



    bts::wallet::pretty_transaction client::wallet_get_pretty_transaction(const bts::blockchain::signed_transaction& transaction) const
    {
      return get_wallet()->to_pretty_trx(wallet_transaction_record(transaction));
    }
    signed_transaction client::wallet_asset_create(const string& symbol,
                                                    const string& asset_name,
                                                    const string& description,
                                                    const fc::variant& data,
                                                    const string& issuer_name,
                                                    share_type maximum_share_supply,
                                                    rpc_client_api::generate_transaction_flag flag)
    {
      bool sign = (flag != do_not_sign);
      auto create_asset_trx = 
        get_wallet()->create_asset(symbol, asset_name, description, data, issuer_name, maximum_share_supply, sign);
      if (flag == sign_and_broadcast)
      {
          network_broadcast_transaction(create_asset_trx);
      }
      return create_asset_trx;
    }

    signed_transaction  client::wallet_asset_issue(share_type amount,
                                                   const string& symbol,
                                                   const string& to_account_name,
                                                   rpc_client_api::generate_transaction_flag flag)
    {
      bool sign = (flag != do_not_sign);
      auto issue_asset_trx = get_wallet()->issue_asset(amount,symbol,to_account_name, sign);
      if (flag == sign_and_broadcast)
      {
          network_broadcast_transaction(issue_asset_trx);
      }
      return issue_asset_trx;
    }

    signed_transaction client::wallet_register_account( const string& account_name,
                                                        const string& pay_with_account,
                                                        const fc::variant& data,
                                                        bool as_delegate,
                                                        rpc_client_api::generate_transaction_flag flag)
    {
      try {
        bool sign = (flag != do_not_sign);
        auto trx = get_wallet()->register_account(account_name, data, as_delegate, pay_with_account, sign);
        if( flag == sign_and_broadcast )
        {
            network_broadcast_transaction(trx);
        }
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("data", data))
    }

    signed_transaction client::wallet_update_registered_account( const string& registered_account_name,
                                                                 const fc::variant& data,
                                                                 bool as_delegate,
                                                                 rpc_client_api::generate_transaction_flag flag )
    {
      FC_ASSERT(!"Not implemented")
      return signed_transaction();
    }


    signed_transaction client::wallet_submit_proposal( const string& delegate_account_name,
                                                       const string& subject,
                                                       const string& body,
                                                       const string& proposal_type,
                                                       const fc::variant& json_data,
                                                       rpc_client_api::generate_transaction_flag flag)
    {
      try {
        bool sign = (flag != do_not_sign);
        auto trx = get_wallet()->create_proposal(delegate_account_name, subject, body, proposal_type, json_data, sign);
        if (flag == sign_and_broadcast)
        {
            network_broadcast_transaction(trx);
        }
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_account_name", delegate_account_name)("subject", subject))
    }

    signed_transaction client::wallet_vote_proposal(const string& name,
                                                     proposal_id_type proposal_id,
                                                     uint8_t vote,
                                                     rpc_client_api::generate_transaction_flag flag)
    {
      try {
        bool sign = (flag != do_not_sign);
        auto trx = get_wallet()->vote_proposal(name, proposal_id, vote, sign);
        if (flag == sign_and_broadcast)
        {
            network_broadcast_transaction(trx);
        }
        return trx;
      } FC_RETHROW_EXCEPTIONS(warn, "", ("name", name)("proposal_id", proposal_id)("vote", vote))
    }



    map<std::string, public_key_type> client::wallet_list_contact_accounts() const
    {
      return get_wallet()->list_contact_accounts();
    }
    map<std::string, public_key_type> client::wallet_list_receive_accounts() const
    {
      return get_wallet()->list_receive_accounts();
    }

    vector<name_record> client::wallet_list_reserved_names(const std::string& account_name) const
    {
      FC_ASSERT(false, "Not Implemented" );
      /*
      auto names = get_wallet()->accounts(account_name);
      vector<name_record> name_records;
      name_records.reserve(names.size());
      for (auto name : names)
        name_records.push_back(name_record(name.second));
      return name_records;
      */
    }

    void client::wallet_rename_account(const string& current_account_name,
                                       const string& new_account_name)
    {
      get_wallet()->rename_account(current_account_name, new_account_name);
    }


    wallet_account_record client::wallet_get_account(const string& account_name) const
    { try {
      auto opt_account = get_wallet()->get_account(account_name);
      if( opt_account.valid() )
         return *opt_account;
      FC_ASSERT(false, "Invalid Account Name: ${account_name}", ("account_name",account_name) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

    vector<wallet_transaction_record> client::wallet_get_transaction_history(unsigned count) const
    {
      return get_wallet()->get_transaction_history();
    }

    vector<pretty_transaction> client::wallet_get_transaction_history_summary(unsigned count) const
    {
        auto tx_recs = get_wallet()->get_transaction_history( );
        auto result = vector<pretty_transaction>();

        for( auto tx_rec : tx_recs)
        {
            result.push_back( get_wallet()->to_pretty_trx( tx_rec ) );
        }

        return result;
    }

    oname_record client::blockchain_get_account_record(const string& name) const
    {
      return get_chain()->get_account_record(name);
    }

    oname_record client::blockchain_get_account_record_by_id(name_id_type name_id) const
    {
      return get_chain()->get_account_record(name_id);
    }

    oasset_record client::blockchain_get_asset_record(const string& symbol) const
    {
      return get_chain()->get_asset_record(symbol);
    }

    oasset_record client::blockchain_get_asset_record_by_id(asset_id_type asset_id) const
    {
      return get_chain()->get_asset_record(asset_id);
    }

    void client::wallet_set_delegate_trust_status(const string& delegate_name, int32_t user_trust_level)
    {
      try {
        auto name_record = get_chain()->get_account_record(delegate_name);
        FC_ASSERT(name_record.valid(), "delegate ${d} does not exist", ("d", delegate_name));
        FC_ASSERT(name_record->is_delegate(), "${d} is not a delegate", ("d", delegate_name));
        FC_ASSERT( !"Not Implemented" );

        //get_wallet()->set_delegate_trust_status(delegate_name, user_trust_level);
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_name", delegate_name)("user_trust_level", user_trust_level))
    }

    /*
    bts::wallet::delegate_trust_status client::wallet_get_delegate_trust_status(const string& delegate_name) const
    {
      try {
        auto name_record = get_chain()->get_account_record(delegate_name);
        FC_ASSERT(name_record.valid(), "delegate ${d} does not exist", ("d", delegate_name));
        FC_ASSERT(name_record->is_delegate(), "${d} is not a delegate", ("d", delegate_name));

        return get_wallet()->get_delegate_trust_status(delegate_name);
      } FC_RETHROW_EXCEPTIONS(warn, "", ("delegate_name", delegate_name))
    }

    map<std::string, bts::wallet::delegate_trust_status> client::wallet_list_delegate_trust_status() const
    {
      return get_wallet()->list_delegate_trust_status();
    }
    */


    osigned_transaction client::blockchain_get_transaction(const transaction_id_type& transaction_id) const
    {
      return get_chain()->get_transaction(transaction_id);
    }

    full_block client::blockchain_get_block(const block_id_type& block_id) const
    {
      return get_chain()->get_block(block_id);
    }

    full_block client::blockchain_get_block_by_number(uint32_t block_number) const
    {
      return get_chain()->get_block(block_number);
    }

    void client::wallet_rescan_blockchain(uint32_t starting_block_number)
    {
      get_wallet()->scan_chain(starting_block_number);
    }

    void client::wallet_rescan_blockchain_state()
    {
      // get_wallet()->scan_state();
    }

    void client::wallet_import_bitcoin(const fc::path& filename,
                                       const string& passphrase,
                                       const string& account_name )
    {
      get_wallet()->import_bitcoin_wallet(filename,passphrase,account_name);
    }

    void client::wallet_import_private_key(const string& wif_key_to_import, 
                                           const string& account_name,
                                           bool wallet_rescan_blockchain)
    {
      get_wallet()->import_wif_private_key(wif_key_to_import, account_name);
      if (wallet_rescan_blockchain)
        get_wallet()->scan_chain(0);
    }

    vector<name_record> client::blockchain_get_names(const std::string& first,
                                                          uint32_t count) const
    {
      return get_chain()->get_names(first, count);
    }

    vector<asset_record> client::blockchain_get_assets(const std::string& first_symbol, uint32_t count) const
    {
      return get_chain()->get_assets(first_symbol,count);
    }

    vector<name_record> client::blockchain_get_delegates(uint32_t first, uint32_t count) const
    {
      auto delegates = get_chain()->get_delegates_by_vote(first, count);
      vector<name_record> delegate_records;
      delegate_records.reserve( delegates.size() );
      for( auto delegate_id : delegates )
        delegate_records.push_back( *get_chain()->get_account_record( delegate_id ) );
      return delegate_records;
    }
    
    std::vector<fc::variant_object> client::network_get_peer_info() const
    {
      std::vector<fc::variant_object> results;
      vector<bts::net::peer_status> peer_statuses = my->_p2p_node->get_connected_peers();
      for (const bts::net::peer_status& peer_status : peer_statuses)
        results.push_back(peer_status.info);
      return results;
    }

    void client::network_set_allowed_peers(const vector<bts::net::node_id_t>& allowed_peers)
    {
      my->_p2p_node->set_allowed_peers(allowed_peers);      
    }

    void client::network_set_advanced_node_parameters(const fc::variant_object& params)
    {
      my->_p2p_node->set_advanced_node_parameters(params);
    }

    fc::variant_object client::network_get_advanced_node_parameters() const
    {
      return my->_p2p_node->get_advanced_node_parameters();
    }

    void client::network_add_node(const fc::ip::endpoint& node, const string& command)
    {
      if (my->_p2p_node)
      {
        if (command == "add")
          my->_p2p_node->add_node(node);
      }
    }

    void client::stop()
    {
      my->_rpc_server->shutdown_rpc_server();
    }

    uint32_t client::network_get_connection_count() const
    {
      return my->_p2p_node->get_connection_count();
    }

    bts::net::message_propagation_data client::network_get_transaction_propagation_data(const transaction_id_type& transaction_id)
    {
      return my->_p2p_node->get_transaction_propagation_data(transaction_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
    }

    bts::net::message_propagation_data client::network_get_block_propagation_data(const block_id_type& block_id)
    {
      return my->_p2p_node->get_block_propagation_data(block_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
    }


    //JSON-RPC Method Implementations END

    void client::run_delegate( )
    {
      my->_delegate_loop_complete = fc::async( [=](){ my->delegate_loop(); } );
    }

    bool client::is_connected() const
    {
      return my->_p2p_node->is_connected();
    }

    fc::uint160_t client::get_node_id() const
    {
      return my->_p2p_node->get_node_id();
    }

    void client::listen_on_port(uint16_t port_to_listen)
    {
        my->_p2p_node->listen_on_port(port_to_listen);
    }

    void client::configure(const fc::path& configuration_directory)
    {
      my->_data_dir = configuration_directory;
      my->_p2p_node->load_configuration( my->_data_dir );
    }

    fc::path client::get_data_dir()const
    {
       return my->_data_dir;
    }
    void client::connect_to_peer(const string& remote_endpoint)

    {
        std::cout << "Attempting to connect to peer " << remote_endpoint << "\n";
        my->_p2p_node->connect_to(fc::ip::endpoint::from_string(remote_endpoint.c_str()));
    }
    void client::connect_to_p2p_network()
    {
      bts::net::item_id head_item_id;
      head_item_id.item_type = bts::client::block_message_type;
      uint32_t last_block_num = my->_chain_db->get_head_block_num();
      if (last_block_num == (uint32_t)-1)
        head_item_id.item_hash = bts::net::item_hash_t();
      else
        head_item_id.item_hash = my->_chain_db->get_head_block_id();
      my->_p2p_node->sync_from(head_item_id);
      my->_p2p_node->connect_to_p2p_network();
    }


    fc::sha256 client_notification::digest()const
    {
      fc::sha256::encoder enc;
      fc::raw::pack(enc, *this);
      return enc.result();
    }

    void client_notification::sign(const fc::ecc::private_key& key)
    {
      signature = key.sign_compact(digest());
    }

    fc::ecc::public_key client_notification::signee() const
    {
      return fc::ecc::public_key(signature, digest());
    }

    balances client::wallet_get_balance( const string& symbol, const std::string& account_name ) const
    { try {
       if( symbol == "*" )
       {
          vector<asset> all_balances = get_wallet()->get_all_balances(account_name);
          balances all_results(all_balances.size());
          for( uint32_t i = 0; i < all_balances.size(); ++i )
          {
             all_results[i].first  = all_balances[i].amount;
             all_results[i].second = get_chain()->get_asset_symbol( all_balances[i].asset_id ); 
          }
          return all_results;
       }
       else
       {
          asset balance = get_wallet()->get_balance( symbol, account_name );
          balances results(1);
          results.back().first = balance.amount;
          results.back().second = get_chain()->get_asset_symbol( balance.asset_id );
          return results;
       }
    } FC_RETHROW_EXCEPTIONS( warn, "", ("symbol",symbol)("account_name",account_name) ) }

    void client::wallet_add_contact_account( const string& account_name, const public_key_type& contact_key )
    {
       get_wallet()->add_contact_account( account_name, contact_key );
    }
    public_key_type client::wallet_create_account( const string& account_name )
    {
       return get_wallet()->create_account( account_name );
    }

    fc::variant_object client::about() const
    {
      fc::mutable_variant_object info;
      info["bitshares_toolkit_revision"]     = bts::utilities::git_revision_sha;
      info["bitshares_toolkit_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(bts::utilities::git_revision_unix_timestamp));
      info["fc_revision"]                    = fc::git_revision_sha;
      info["fc_revision_age"]                = fc::get_approximate_relative_time_string(fc::time_point_sec(fc::git_revision_unix_timestamp));
      info["compile_date"]                   = "compiled on " __DATE__ " at " __TIME__;
      return info;
    }

    std::string client::help(const std::string& command_name) const
    {
      return get_rpc_server()->help(command_name);
    }

} } // bts::client
