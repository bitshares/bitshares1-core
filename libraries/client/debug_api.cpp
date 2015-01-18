#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/wallet/wallet.hpp>

namespace bts { namespace client { namespace detail {

void client_impl::debug_enable_output(bool enable_flag)
{
   _cli->enable_output(enable_flag);
}

void client_impl::debug_filter_output_for_tests(bool enable_flag)
{
   _cli->filter_output_for_tests(enable_flag);
}

void client_impl::debug_wait(uint32_t wait_time) const
{
   fc::usleep(fc::seconds(wait_time));
}

void client_impl::debug_wait_block_interval(uint32_t wait_time) const
{
   fc::usleep(fc::seconds(wait_time*BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC));
}

map<fc::time_point, fc::exception> client_impl::debug_list_errors( int32_t first_error_number, uint32_t limit, const string& filename )const
{
   map<fc::time_point, fc::exception> result;
   int count = 0;
   auto itr = _exception_db.begin();
   while( itr.valid() )
   {
      ++count;
      if( count > first_error_number )
      {
         result[itr.key()] = itr.value();
         if (--limit == 0)
            break;
      }
      ++itr;
   }

   if( filename != "" )
   {
      fc::path file_path( filename );
      FC_ASSERT( !fc::exists( file_path ) );
      std::ofstream out( filename.c_str() );
      for( auto item : result )
      {
         out << std::string(item.first) << "  " << item.second.to_detail_string() <<"\n-----------------------------\n";
      }
   }
   return result;
}

map<fc::time_point, std::string> client_impl::debug_list_errors_brief( int32_t first_error_number, uint32_t limit, const string& filename ) const
{
   map<fc::time_point, fc::exception> full_errors = debug_list_errors(first_error_number, limit, "");

   map<fc::time_point, std::string> brief_errors;
   for (auto full_error : full_errors)
      brief_errors.insert(std::make_pair(full_error.first, full_error.second.what()));

   if( filename != "" )
   {
      fc::path file_path( filename );
      FC_ASSERT( !fc::exists( file_path ) );
      std::ofstream out( filename.c_str() );
      for( auto item : brief_errors )
         out << std::string(item.first) << "  " << item.second <<"\n";
   }

   return brief_errors;
}

void client_impl::debug_clear_errors( const fc::time_point& start_time, int32_t first_error_number, uint32_t limit )
{
   auto itr = _exception_db.lower_bound( start_time );
   //skip to first error to clear
   while (itr.valid() && first_error_number > 1)
   {
      --first_error_number;
      ++itr;
   }
   //clear the desired errors
   while( itr.valid() && limit > 0)
   {
      _exception_db.remove(itr.key());
      --limit;
      ++itr;
   }
}

void client_impl::debug_write_errors_to_file( const string& path, const fc::time_point& start_time )const
{
   map<fc::time_point, fc::exception> result;
   auto itr = _exception_db.lower_bound( start_time );
   while( itr.valid() )
   {
      result[itr.key()] = itr.value();
      ++itr;
   }
   if (path != "")
   {
      std::ofstream fileout( path.c_str() );
      fileout << fc::json::to_pretty_string( result );
   }
}

fc::variant_object client_impl::debug_get_call_statistics() const
{
   return _p2p_node->get_call_statistics();
}

fc::variant_object client_impl::debug_verify_delegate_votes() const
{
   return _chain_db->find_delegate_vote_discrepancies();
}

void client_impl::debug_start_simulated_time(const fc::time_point& starting_time)
{
   bts::blockchain::start_simulated_time(starting_time);
}

void client_impl::debug_advance_time(int32_t delta_time, const std::string& unit /* = "seconds" */)
{
   if ((unit == "block") || (unit == "blocks"))
      delta_time *= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   else if ((unit == "round") || (unit == "rounds"))
      delta_time *= BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   else if ((unit != "second") && (unit != "seconds"))
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "unit must be \"second(s)\", \"block(s)\", or \"round(s)\", was: \"${unit}\"", ("unit", unit));
   bts::blockchain::advance_time(delta_time);
}

void client_impl::debug_wait_for_block_by_number(uint32_t block_number, const std::string& type /* = "absolute" */)
{
   if (type == "relative")
      block_number += _chain_db->get_head_block_num();
   else if (type == "rlast")
   {
      _debug_last_wait_block += block_number;
      block_number = _debug_last_wait_block;
   }
   else if (type != "absolute")
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "type must be \"absolute\", \"rlast\" or \"relative\", was: \"${type}\"", ("type", type));
   if (_chain_db->get_head_block_num() >= block_number)
      return;
   fc::promise<void>::ptr block_arrived_promise(new fc::promise<void>("debug_wait_for_block_by_number"));
   class wait_for_block : public bts::blockchain::chain_observer
   {
      uint32_t               _block_number;
      fc::promise<void>::ptr _completion_promise;
   public:
      wait_for_block(uint32_t block_number, fc::promise<void>::ptr completion_promise) :
         _block_number(block_number),
         _completion_promise(completion_promise)
      {}
      void state_changed(const pending_chain_state_ptr& state) override {}
      void block_applied(const block_summary& summary) override
      {
         if (summary.block_data.block_num >= _block_number)
            _completion_promise->set_value();
      }
   };
   wait_for_block block_waiter(block_number, block_arrived_promise);
   _chain_db->add_observer(&block_waiter);
   try
   {
      block_arrived_promise->wait();
   }
   catch (...)
   {
      _chain_db->remove_observer(&block_waiter);
      throw;
   }
   _chain_db->remove_observer(&block_waiter);
}

std::string client_impl::debug_get_client_name() const
{
   return this->_config.client_debug_name;
}

static std::string _generate_deterministic_private_key(const std::string& prefix, int32_t index)
{
   std::string seed = prefix;
   if( index >= 0 )
      seed += std::to_string(index);
   fc::sha256 h_seed = fc::sha256::hash(seed);
   fc::ecc::private_key key = fc::ecc::private_key::regenerate(h_seed);
   return bts::utilities::key_to_wif(key);
}

fc::variants client_impl::debug_deterministic_private_keys(
   int32_t start,
   int32_t count,
   const std::string& prefix,
   bool import,
   const std::string& account_name,
   bool create_account,
   bool wallet_rescan_blockchain
   )
{
   std::vector<std::string> generated_keys;
   
   if( start < 0 )
   {
      // ignore count, generate single key
      generated_keys.push_back(_generate_deterministic_private_key(prefix, -1));
   }
   else
   {
      for( int32_t i=0;i<count;i++ )
      {
         int64_t sum = start;
         sum += i;
         generated_keys.push_back(_generate_deterministic_private_key(prefix, sum));
      }
   }

   fc::variants result;
   for( const fc::string &s : generated_keys )
      result.push_back( s );

   if( !import )
      return result;

   optional<string> name;
   if( !account_name.empty() )
      name = account_name;

   for( const std::string& k : generated_keys )
   {
      _wallet->import_wif_private_key( k, name, create_account );
      create_account = false;
   }
   if( wallet_rescan_blockchain )
      _wallet->start_scan( 0, -1 );

   return result;
}

} } } // namespace bts::client::detail
