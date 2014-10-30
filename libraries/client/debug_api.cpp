#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>

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
   if (unit == "blocks")
      delta_time *= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   else if (unit == "rounds")
      delta_time *= BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   else if (unit != "seconds")
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "unit must be \"seconds\", \"blocks\", or \"rounds\", was: \"${unit}\"", ("unit", unit));
   bts::blockchain::advance_time(delta_time);
}

void client_impl::debug_wait_for_block_by_number(uint32_t block_number, const std::string& type /* = "absolute" */)
{
   if (type == "relative")
      block_number += _chain_db->get_head_block_num();
   else if (type != "absolute")
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "type must be \"absolute\", or \"relative\", was: \"${type}\"", ("type", type));
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

} } } // namespace bts::client::detail
