#include <algorithm>

#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/net/node.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

#include <iostream>

namespace bts { namespace client {

    namespace detail
    {
       class client_impl : public bts::net::node_delegate
       {
          public:
            client_impl()
            {
                _p2p_node = std::make_shared<bts::net::node>();
                _p2p_node->set_delegate(this);
            }

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
            virtual std::vector<bts::net::item_hash_t> get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit = 2000) override;
            virtual bts::net::message get_item(const bts::net::item_id& id) override;
            virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
            virtual void connection_count_changed(uint32_t c) override;
            /// @}

            fc::time_point                                              _last_block;
            fc::path                                                    _data_dir;

            bts::net::node_ptr                                          _p2p_node;
            bts::blockchain::chain_database_ptr                         _chain_db;
            std::unordered_map<transaction_id_type, signed_transaction> _pending_trxs;
            bts::wallet::wallet_ptr                                     _wallet;
            fc::future<void>                                            _delegate_loop_complete;
       };

       void client_impl::delegate_loop()
       {
          fc::usleep( fc::seconds( 10 ) );
         _last_block = _chain_db->get_head_block().timestamp;
         while( !_delegate_loop_complete.canceled() )
         {
            auto now = fc::time_point::now();
            auto next_block_time = _wallet->next_block_production_time();
            ilog( "next block time: ${b}  interval: ${i} seconds", 
                  ("b",next_block_time)("i",BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
            if( next_block_time < now || 
                (next_block_time - now) > fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) )
            {
               fc::usleep( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) );
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
                     _p2p_node->broadcast(block_message(next_block.id(), next_block, 
                                                        next_block.signee ));
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
            fc::usleep( fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC/2) );
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

       /**
        *  Get the hash of all blocks after from_id
        */
       std::vector<bts::net::item_hash_t> client_impl::get_item_ids(const bts::net::item_id& from_id,
                                                                    uint32_t& remaining_item_count,
                                                                    uint32_t limit /* = 2000 */)
       {
         FC_ASSERT(from_id.item_type == bts::client::block_message_type);
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
             return std::vector<bts::net::item_hash_t>();
           }
         }
         remaining_item_count = _chain_db->get_head_block_num() - last_seen_block_num;
         uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
         std::vector<bts::net::item_hash_t> hashes_to_return;
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

       bts::net::message client_impl::get_item(const bts::net::item_id& id)
       {
         if (id.item_type == block_message_type)
         {
        //   uint32_t block_number = _chain_db->get_block_num(id.item_hash);
           bts::client::block_message block_message_to_send;
           block_message_to_send.block = _chain_db->get_block(id.item_hash);
           block_message_to_send.block_id = block_message_to_send.block.id();
           FC_ASSERT(id.item_hash == block_message_to_send.block_id);
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
    :my( new detail::client_impl() )
    {
    }

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

    void client::set_chain( const bts::blockchain::chain_database_ptr& ptr )
    {
       my->_chain_db = ptr;
    }

    void client::set_wallet( const bts::wallet::wallet_ptr& wall )
    {
       FC_ASSERT( my->_chain_db );
       my->_wallet = wall;
    }

    bts::wallet::wallet_ptr client::get_wallet()const { return my->_wallet; }
    bts::blockchain::chain_database_ptr client::get_chain()const { return my->_chain_db; }
    bts::net::node_ptr client::get_node()const { return my->_p2p_node; }
    signed_transactions client::get_pending_transactions()const { return my->get_pending_transactions(); }

    void client::broadcast_transaction( const signed_transaction& trx )
    {
      ilog("broadcasting transaction with id ${id} : ${transaction}", ("id", trx.id())("transaction", trx));

      // p2p doesn't send messages back to the originator
      my->on_new_transaction(trx);
      my->_p2p_node->broadcast(trx_message(trx));
    }

    void client::run_delegate( )
    {
       my->_delegate_loop_complete = fc::async( [=](){ my->delegate_loop(); } );
    }

    bool client::is_connected() const
    {
        return my->_p2p_node->is_connected();
    }

    uint32_t client::get_connection_count() const
    {
       return my->_p2p_node->get_connection_count();
    }

    fc::variants client::get_peer_info() const
    {
      fc::variants results;
        std::vector<bts::net::peer_status> peer_statuses = my->_p2p_node->get_connected_peers();
        for (const bts::net::peer_status& peer_status : peer_statuses)
        {
          results.push_back(peer_status.info);
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
        return my->_p2p_node->get_transaction_propagation_data(transaction_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
    }

    bts::net::message_propagation_data client::get_block_propagation_data(const bts::blockchain::block_id_type& block_id)
    {
        return my->_p2p_node->get_block_propagation_data(block_id);
      FC_THROW_EXCEPTION(invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
    }

    fc::uint160_t client::get_node_id() const
    {
      return my->_p2p_node->get_node_id();      
    }

    void client::set_advanced_node_parameters(const fc::variant_object& params)
    {
        my->_p2p_node->set_advanced_node_parameters(params);
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
    void client::connect_to_peer(const std::string& remote_endpoint)

    {
       std::cout << "Attempting to conenct to peer " << remote_endpoint << "\n";
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


    transaction_id_type client::reserve_name( const std::string& name, const fc::variant& data )
    { try {
        auto trx = get_wallet()->reserve_name( name, data );
        broadcast_transaction( trx );
        return trx.id();
    } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("data",data) ) }
    transaction_id_type client::register_delegate( const std::string& name, const fc::variant& data )
    { try {
             FC_ASSERT( !"Not Implemented" );
        //auto trx = get_wallet()->register_delegate( name, data );
        //broadcast_transaction( trx );
       // return trx.id();
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
