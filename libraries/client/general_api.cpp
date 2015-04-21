#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>

namespace bts { namespace client { namespace detail {

fc::variant_object client_impl::about() const
{
   return bts::client::version_info();
}

string client_impl::help(const string& command_name) const
{
   return _rpc_server->help(command_name);
}

method_map_type client_impl::meta_help() const
{
   return _rpc_server->meta_help();
}

variant_object client_impl::get_info()const
{
   const auto now = blockchain::now();
   auto info = fc::mutable_variant_object();

   /* Blockchain */
   const auto head_block_num                                    = _chain_db->get_head_block_num();
   info["blockchain_head_block_num"]                            = head_block_num;
   info["blockchain_head_block_age"]                            = variant();
   info["blockchain_head_block_timestamp"]                      = variant();
   info["blockchain_head_block_id"]                             = variant();
   time_point_sec head_block_timestamp;
   if( head_block_num > 0 )
   {
      head_block_timestamp                                      = _chain_db->now();
      info["blockchain_head_block_age"]                         = ( now - head_block_timestamp ).to_seconds();
      info["blockchain_head_block_timestamp"]                   = head_block_timestamp;
      info["blockchain_head_block_id"]                          = _chain_db->get_head_block_id();
   }

   info["blockchain_average_delegate_participation"]            = variant();
   const auto participation                                     = _chain_db->get_average_delegate_participation();
   if( participation <= 100 )
      info["blockchain_average_delegate_participation"]         = participation;

   info["blockchain_share_supply"]                              = variant();
   const auto share_record                                      = _chain_db->get_asset_record( BTS_BLOCKCHAIN_SYMBOL );
   if( share_record.valid() )
      info["blockchain_share_supply"]                           = share_record->current_supply;

   const auto blocks_left                                       = BTS_BLOCKCHAIN_NUM_DELEGATES - (head_block_num % BTS_BLOCKCHAIN_NUM_DELEGATES);
   info["blockchain_blocks_left_in_round"]                      = blocks_left;

   info["blockchain_next_round_time"]                           = variant();
   info["blockchain_next_round_timestamp"]                      = variant();
   if( head_block_num > 0 )
   {
      const auto current_round_timestamp                        = blockchain::get_slot_start_time( now );
      const auto next_round_timestamp                           = current_round_timestamp + (blocks_left * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
      info["blockchain_next_round_time"]                        = ( next_round_timestamp - now ).to_seconds();
      info["blockchain_next_round_timestamp"]                   = next_round_timestamp;
   }

   info["blockchain_random_seed"]                               = _chain_db->get_current_random_seed();

   /* Client */
   info["client_data_dir"]                                      = fc::absolute( _data_dir );
   info["client_version"]                                       = bts::client::version_info()["client_version"].as_string();

   /* Network */
   info["network_num_connections"]                              = network_get_connection_count();
   fc::variant_object advanced_params                           = network_get_advanced_node_parameters();
   info["network_num_connections_max"]                          = advanced_params["maximum_number_of_connections"];
   info["network_chain_downloader_running"]                     = _chain_downloader_running;
   info["network_chain_downloader_blocks_remaining"]            = _chain_downloader_running?
            _chain_downloader_blocks_remaining :
            variant();

   /* NTP */
   info["ntp_time"]                                             = variant();
   info["ntp_time_error"]                                       = variant();
   if( blockchain::ntp_time().valid() )
   {
      info["ntp_time"]                                          = now;
      info["ntp_time_error"]                                    = static_cast<double>(blockchain::ntp_error().count()) / fc::seconds(1).count();
   }

   /* Wallet */
   const auto is_open                                           = _wallet->is_open();
   info["wallet_open"]                                          = is_open;

   info["wallet_unlocked"]                                      = variant();
   info["wallet_unlocked_until"]                                = variant();
   info["wallet_unlocked_until_timestamp"]                      = variant();

   info["wallet_last_scanned_block_timestamp"]                  = variant();
   info["wallet_scan_progress"]                                 = variant();

   info["wallet_block_production_enabled"]                      = variant();
   info["wallet_next_block_production_time"]                    = variant();
   info["wallet_next_block_production_timestamp"]               = variant();

   if( is_open )
   {
      info["wallet_unlocked"]                                   = _wallet->is_unlocked();

      const auto unlocked_until                                 = _wallet->unlocked_until();
      if( unlocked_until.valid() )
      {
         info["wallet_unlocked_until"]                          = ( *unlocked_until - now ).to_seconds();
         info["wallet_unlocked_until_timestamp"]                = *unlocked_until;

         const auto last_scanned_block_num                      = _wallet->get_last_scanned_block_number();
         if( last_scanned_block_num > 0 )
         {
            try
            {
               info["wallet_last_scanned_block_timestamp"]      = _chain_db->get_block_header( last_scanned_block_num ).timestamp;
            }
            catch (fc::canceled_exception&)
            {
               throw;
            }
            catch( ... )
            {
            }
         }

         info["wallet_scan_progress"]                           = _wallet->get_scan_progress();

         const auto enabled_delegates                           = _wallet->get_my_delegates( enabled_delegate_status );
         const auto block_production_enabled                    = !enabled_delegates.empty();
         info["wallet_block_production_enabled"]                = block_production_enabled;

         if( block_production_enabled )
         {
            const auto next_block_timestamp                     = _wallet->get_next_producible_block_timestamp( enabled_delegates );
            if( next_block_timestamp.valid() )
            {
               info["wallet_next_block_production_time"]        = ( *next_block_timestamp - now ).to_seconds();
               info["wallet_next_block_production_timestamp"]   = *next_block_timestamp;
            }
         }
      }
   }

   return info;
}

address client_impl::convert_to_native_address( const string& raw_address)const
{
    auto pts_addr = pts_address(raw_address);
    return address(pts_addr);
}

fc::variant_object client_impl::validate_address(const string& address) const
{
   fc::mutable_variant_object result;
   try
   {
      bts::blockchain::public_key_type test_key(address);
      result["isvalid"] = true;
   }
   catch (const fc::exception&)
   {
      result["isvalid"] = false;
   }
   return result;
}

string client_impl::execute_command_line(const string& input) const
{
   if (_cli)
   {
      std::stringstream output;
      _cli->execute_command_line( input, &output );
      return output.str();
   }
   else
   {
      return "CLI not set for this client.\n";
   }
}

fc::variants client_impl::batch( const std::string& method_name,
                                 const std::vector<fc::variants>& parameters_list) const
{
   FC_ASSERT( _self->get_rpc_server()->get_method_data(method_name).prerequisites == bts::api::no_prerequisites,
              "${method_name}", ("method_name", method_name) );

   return batch_authenticated(method_name, parameters_list);
}

fc::variants client_impl::batch_authenticated( const std::string& method_name,
                                 const std::vector<fc::variants>& parameters_list) const
{
   fc::variants result;
   for ( auto parameters : parameters_list )
   {
      try {
          result.push_back( _self->get_rpc_server()->direct_invoke_method( method_name, parameters) );
      } catch (const fc::exception& e) {
          fc::mutable_variant_object resp;
          resp["error"] = e;
          result.push_back( resp );
      }
   }
   return result;
}

wallet_transaction_record  client_impl::builder_finalize_and_sign( const transaction_builder& builder )const
{
    auto b = builder;
    b.finalize();
    return b.sign();
}


void detail::client_impl::stop()
{
   elog( "stop...");
   _p2p_node->close();
   _rpc_server->shutdown_rpc_server();
}

void detail::client_impl::rpc_set_username(const string& username)
{
   std::cout << "rpc_set_username(" << username << ")\n";
   _tmp_rpc_config.rpc_user = username;
}

void detail::client_impl::rpc_set_password(const string& password)
{
   std::cout << "rpc_set_password(********)\n";
   _tmp_rpc_config.rpc_password = password;
}

void detail::client_impl::rpc_start_server(const uint32_t port)
{
   std::cout << "rpc_server_port: " << port << "\n";
   _tmp_rpc_config.rpc_endpoint.set_port(port);
   if(_tmp_rpc_config.is_valid())
   {
      std::cout << "starting rpc server\n";
      _rpc_server->configure_rpc(_tmp_rpc_config);
   }
   else
      std::cout << "RPC server was not started, configuration error\n";
}

void detail::client_impl::http_start_server(const uint32_t port)
{
   std::cout << "http_server_port: " << port << "\n";
   _tmp_rpc_config.httpd_endpoint.set_port(port);
   if(_tmp_rpc_config.is_valid())
   {
      std::cout << "starting http server\n";
      _rpc_server->configure_http(_tmp_rpc_config);
   }
   else
      std::cout << "Http server was not started, configuration error\n";
}

void detail::client_impl::ntp_update_time()
{
   blockchain::ntp_time(); // this is just called to ensure the NTP time service is running
   blockchain::update_ntp_time();
}

variant detail::client_impl::disk_usage()const
{
   fc::mutable_variant_object usage;

   usage["blockchain"] = variant();
   usage["dac_state"] = variant();
   usage["logs"] = variant();
   usage["network_peers"] = variant();
   usage["wallets"] = variant();
   usage["total"] = variant();

   const fc::path blockchain = _data_dir / "chain" / "raw_chain";
   usage["blockchain"] = fc::is_directory( blockchain ) ? fc::directory_size( blockchain ) : variant();

   const fc::path dac_state = _data_dir / "chain" / "index";
   usage["dac_state"] = fc::is_directory( dac_state ) ? fc::directory_size( dac_state ) : variant();

   const fc::path logs = _data_dir / "logs";
   usage["logs"] = fc::is_directory( logs ) ? fc::directory_size( logs ) : variant();

   const fc::path network_peers = _data_dir / "peers.leveldb";
   usage["network_peers"] = fc::is_directory( network_peers ) ? fc::directory_size( network_peers ) : variant();

   fc::mutable_variant_object wallet_sizes;

   const fc::path wallets = _data_dir / "wallets";

   const fc::path backups = wallets / ".backups";
   if( fc::is_directory( backups ) )
      wallet_sizes[".backups"] = fc::directory_size( backups );

   for( const string& wallet_name : _wallet->list() )
      wallet_sizes[wallet_name] = fc::directory_size( wallets / wallet_name );

   usage["wallets"] = wallet_sizes;

   usage["total"] = fc::is_directory( _data_dir ) ? fc::directory_size( _data_dir ) : variant();

   return usage;
}

} } } // namespace bts::client::detail
