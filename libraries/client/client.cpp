#include <algorithm>

#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/chain_client.hpp>
#include <bts/net/node.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace client {

    namespace detail
    {
       class client_impl : public bts::net::chain_client_delegate,
                           public bts::net::node_delegate
       {
          public:
            client_impl(bool use_p2p = false)
            {
              if (use_p2p)
              {
                _p2p_node = std::make_shared<bts::net::node>();
                _p2p_node->set_delegate(this);
              }
              else
              {
                // use server-based implementation
                _chain_client = std::make_shared<bts::net::chain_client>();
                _chain_client->set_delegate(this);
              }
            }

            void trustee_loop();
            signed_transactions get_pending_transactions() const;

            /* Implement chain_client_impl */
            // @{
            virtual void on_new_block(const trx_block& block) override;
            virtual void on_new_transaction(const signed_transaction& trx) override;
            /// @}

            /* Implement node_delegate */
            // @{
            virtual bool has_item(const bts::net::item_id& id) override;
            virtual void handle_message(const bts::net::message&) override;
            virtual std::vector<bts::net::item_hash_t> get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit = 2000) override;
            virtual bts::net::message get_item(const bts::net::item_id& id) override;
            virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
            virtual void connection_count_changed(uint32_t c) override;
            /// @}

            fc::ecc::private_key                                        _trustee_key;
            fc::time_point                                              _last_block;
            fc::path                                                    _data_dir;

            bts::blockchain::trx_block                                  _next_block;
            bts::net::chain_client_ptr                                  _chain_client;
            bts::net::node_ptr                                          _p2p_node;
            bts::blockchain::chain_database_ptr                         _chain_db;
            std::unordered_map<transaction_id_type, signed_transaction> _pending_trxs;
            bts::wallet::wallet_ptr                                     _wallet;
            fc::future<void>                                            _trustee_loop_complete;
       };

       void client_impl::trustee_loop()
       {
          fc::usleep( fc::seconds( 10 ) );
         _last_block = _chain_db->get_head_block().timestamp;
         while( !_trustee_loop_complete.canceled() )
         {
           signed_transactions pending_trxs;
           pending_trxs = get_pending_transactions();
           _last_block = _chain_db->get_head_block().timestamp;
           if( (fc::time_point::now() - _last_block) > fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) )
           {
             try {
               bts::blockchain::trx_block blk = _wallet->generate_next_block(*_chain_db, pending_trxs);
               blk.sign(_trustee_key);
               // _chain_db->push_block( blk );
               if (_chain_client)
               {
                 on_new_block(blk);
                 _chain_client->broadcast_block(blk);
               }
               else
               {
                 // with the p2p code, if you broadcast something to the network, it will not
                 // immediately send it back to you
                 on_new_block(blk);
                 _p2p_node->broadcast(block_message(blk.id(), blk, blk.trustee_signature));
               }
               _last_block = fc::time_point::now();
             }
             catch( const fc::exception& e )
             {
               elog("error producing block?: ${e}", ("e", e.to_detail_string()));
             }
           }
           fc::usleep(fc::seconds(1));
         }
       } // trustee_loop

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
       void client_impl::on_new_block(const trx_block& block)
       {
         try
         {
           ilog("Received a new block from the server, current head block is ${num}, block is ${block}", ("num", _chain_db->head_block_num())("block", block));
           if (block.id() == _chain_db->head_block_id())
             ilog("Ignoring this block, I already have it.  That probably means I'm the trustee who generated the block");
           else
             _chain_db->push_block(block);
         }
         catch (fc::exception& e)
         {
           wlog("Error pushing block ${block}: ${error}", ("block", block)("error", e.to_string()));
           throw;
         }

         for (auto trx : block.trxs)
           _pending_trxs.erase(trx.id());
         ilog("");
         _wallet->scan_chain(*_chain_db, block.block_num);
       }

       void client_impl::on_new_transaction(const signed_transaction& trx)
       {
         _chain_db->evaluate_transaction(trx); // throws exception if invalid trx.
         if (_pending_trxs.insert(std::make_pair(trx.id(), trx)).second)
           ilog("new transaction with id ${id} : ${transaction}", ("id", trx.id())("transaction", trx));
         else
         {
           trx_message original_trx_message(trx);
           bts::net::message original_message(original_trx_message);
           wlog("duplicate transaction with id ${id}, should have arrived in message ${message_id}, ignoring", 
                ("id", trx.id())("message_id", original_message.id()));
         }
       }


       ///////////////////////////////////////////////////////
       // Implement node_delegate                           //
       ///////////////////////////////////////////////////////
       bool client_impl::has_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
           try
           {
             _chain_db->fetch_block_num(id.item_hash);
             return true;
           }
           catch (const fc::key_not_found_exception&)
           {
             return false;
           }
         }

         if (id.item_type == trx_message_type)
         {
           auto iter = _pending_trxs.find(id.item_hash);
           return iter != _pending_trxs.end();
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
             ilog("CLIENT: just received block ${id}", ("id", block_message_to_handle.block_id));
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

       std::vector<bts::net::item_hash_t> client_impl::get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit /* = 2000 */)
       {
         FC_ASSERT(from_id.item_type == bts::client::block_message_type);
         uint32_t last_seen_block_num;
         try
         {
           last_seen_block_num = _chain_db->fetch_block_num(from_id.item_hash);
         }
         catch (fc::key_not_found_exception&)
         {
           if (from_id.item_hash == bts::net::item_hash_t())
             last_seen_block_num = (uint32_t)-1;
           else
           {
             remaining_item_count = 0;
             return std::vector<bts::net::item_hash_t>();
           }
         }
         remaining_item_count = _chain_db->head_block_num() - last_seen_block_num;
         uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
         std::vector<bts::net::item_hash_t> hashes_to_return;
         hashes_to_return.reserve(items_to_get_this_iteration);
         for (uint32_t i = 0; i < items_to_get_this_iteration; ++i)
         {
           ++last_seen_block_num;
           signed_block_header header;
           try
           {
             header = _chain_db->fetch_block(last_seen_block_num);
           }
           catch (fc::key_not_found_exception&)
           {
             assert(false && "I assume this can never happen");
           }
           hashes_to_return.push_back(header.id());
         }
         remaining_item_count -= items_to_get_this_iteration;
         return hashes_to_return;
       }

       bts::net::message client_impl::get_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
           uint32_t block_number = _chain_db->fetch_block_num(id.item_hash);
           bts::client::block_message block_message_to_send;
           block_message_to_send.block = _chain_db->fetch_trx_block(block_number);
           block_message_to_send.block_id = block_message_to_send.block.id();
           FC_ASSERT(id.item_hash == block_message_to_send.block_id);
           block_message_to_send.signature = block_message_to_send.block.trustee_signature;
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

    client::client(bool enable_p2p /* = false */)
    :my( new detail::client_impl(enable_p2p) )
    {
    }

    client::~client()
    {
       try {
          if( my->_trustee_loop_complete.valid() )
          {
             my->_trustee_loop_complete.cancel();
             ilog( "waiting for trustee loop to complete" );
             my->_trustee_loop_complete.wait();
          }
       }
       catch ( const fc::canceled_exception& ) {}
       catch ( const fc::exception& e )
       {
          wlog( "${e}", ("e",e.to_detail_string() ) );
       }
    }

    void client::set_chain( const bts::blockchain::chain_database_ptr& ptr )
    {
       my->_chain_db = ptr;
       if (my->_chain_client)
         my->_chain_client->set_chain( ptr );
    }

    void client::set_wallet( const bts::wallet::wallet_ptr& wall )
    {
       FC_ASSERT( my->_chain_db );
       my->_wallet = wall;
       my->_wallet->scan_chain( *my->_chain_db, my->_chain_db->head_block_num() );
    }

    bts::wallet::wallet_ptr client::get_wallet()const { return my->_wallet; }
    bts::blockchain::chain_database_ptr client::get_chain()const { return my->_chain_db; }
    bts::net::node_ptr client::get_node()const { return my->_p2p_node; }
    signed_transactions client::get_pending_transactions()const { return my->get_pending_transactions(); }

    void client::broadcast_transaction( const signed_transaction& trx )
    {
      ilog("broadcasting transaction with id ${id} : ${transaction}", ("id", trx.id())("transaction", trx));
      if (my->_chain_client)
        my->_chain_client->broadcast_transaction( trx );
      else
      {
        // p2p doesn't send messages back to the originator
        my->on_new_transaction(trx);
        my->_p2p_node->broadcast(trx_message(trx));
      }
    }

    void client::add_node( const std::string& ep )
    {
      if (my->_chain_client)
        my->_chain_client->add_node(ep);
    }

    void client::run_trustee( const fc::ecc::private_key& k )
    {
       my->_trustee_key = k;
       my->_trustee_loop_complete = fc::async( [=](){ my->trustee_loop(); } );
    }

    bool client::is_connected() const
    {
      if (my->_chain_client)
        return my->_chain_client->is_connected();
      else
        return my->_p2p_node->is_connected();
    }

    uint32_t client::get_connection_count() const
    {
      if (my->_chain_client)
        return my->_chain_client->is_connected() ? 1 : 0;
      else
        return my->_p2p_node->get_connection_count();
    }

    fc::variants client::get_peer_info() const
    {
      fc::variants results;
      if (my->_p2p_node)
      {
        std::vector<bts::net::peer_status> peer_statuses = my->_p2p_node->get_connected_peers();
        for (const bts::net::peer_status& peer_status : peer_statuses)
        {
          results.push_back(peer_status.info);
        }
      }
      else
      {
        if (my->_chain_client->is_connected())
        {
          // fake up some data.  Since we aren't likely to keep the chain_client code
          // around, we won't go through the trouble of making it accurate yet
          fc::mutable_variant_object peer_details;
          peer_details["addr"] = "127.0.0.1:1234";
          peer_details["addrlocal"] = "127.0.0.1:1234";
          peer_details["services"] = "00000001";
          peer_details["lastsend"] = "";
          peer_details["lastrecv"] = "";
          peer_details["bytessent"] = "0";
          peer_details["bytesrecv"] = "0";
          peer_details["conntime"] = "";
          peer_details["pingtime"] = "";
          peer_details["pingwait"] = "";
          peer_details["version"] = "";
          peer_details["subver"] = "";
          peer_details["inbound"] = false;
          peer_details["startingheight"] = "";
          peer_details["banscore"] = "";
          peer_details["syncnode"] = "";
          results.push_back(peer_details);
        }
      }
      return results;
    }

    void client::addnode(const fc::ip::endpoint& node, const std::string& command)
    {
      if (my->_p2p_node)
      {
        if (command == "add")
          my->_p2p_node->add_node(node);          
      }
    }

    void client::stop()
    {
    }

    bts::net::message_propagation_data client::get_transaction_propagation_data(const bts::blockchain::transaction_id_type& transaction_id)
    {
      if (my->_p2p_node)
        return my->_p2p_node->get_transaction_propagation_data(transaction_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
    }

    bts::net::message_propagation_data client::get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
    {
      if (my->_p2p_node)
        return my->_p2p_node->get_block_propagation_data(block_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
    }

    fc::uint160_t client::get_node_id() const
    {
      if (my->_p2p_node)
        return my->_p2p_node->get_node_id();      
      return fc::uint160_t();
    }

    void client::set_advanced_node_parameters(const fc::variant_object& params)
    {
      if (my->_p2p_node)
        my->_p2p_node->set_advanced_node_parameters(params);
    }

    void client::listen_on_port(uint16_t port_to_listen)
    {
      if (my->_p2p_node)
        my->_p2p_node->listen_on_port(port_to_listen);
    }

    void client::configure(const fc::path& configuration_directory)
    {
      my->_data_dir = configuration_directory;
      if (my->_p2p_node)
        my->_p2p_node->load_configuration( my->_data_dir );
    }

    fc::path client::get_data_dir()const
    {
       return my->_data_dir;
    }
    void client::connect_to_peer(const std::string& remote_endpoint)

    {
      if (my->_p2p_node)
        my->_p2p_node->connect_to(fc::ip::endpoint::from_string(remote_endpoint.c_str()));
    }
    void client::connect_to_p2p_network()
    {
      if (!my->_p2p_node)
        return;
      bts::net::item_id head_item_id;
      head_item_id.item_type = bts::client::block_message_type;
      uint32_t last_block_num = my->_chain_db->head_block_num();
      if (last_block_num == (uint32_t)-1)
        head_item_id.item_hash = bts::net::item_hash_t();
      else
        head_item_id.item_hash = my->_chain_db->head_block_id();
      my->_p2p_node->sync_from(head_item_id);
      my->_p2p_node->connect_to_p2p_network();
    }


    transaction_id_type client::reserve_name( const std::string& name, const fc::variant& data )
    { try {
        auto trx = get_wallet()->reserve_name( name, data );
        broadcast_transaction( trx );
        return trx.id();
    } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("data",data) ) }
    transaction_id_type client::register_delegate( const std::string& name, const fc::variant& data )
    { try {
        auto trx = get_wallet()->register_delegate( name, data );
        broadcast_transaction( trx );
        return trx.id();
    } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("data",data) ) }

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

} } // bts::client
