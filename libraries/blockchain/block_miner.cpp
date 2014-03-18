#include <bts/blockchain/block_miner.hpp>
#include <bts/blockchain/momentum.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {

  namespace detail 
  { 
     class block_miner_impl
     {
        public:
           block_miner_impl()
           :_target(0),_miner_votes(0),_min_votes(0),_effort(0){}

           block_miner::callback _callback;
           fc::thread*           _main_thread;
           fc::thread            _mining_thread;
           uint64_t              _target;
           uint64_t              _miner_votes;
           uint64_t              _min_votes;
           block_header          _current_block;
           fc::future<void>      _mining_loop_complete;
           float                 _effort;

           void mining_loop()
           {
              while( !_mining_loop_complete.canceled() )
              {
                 if( !_callback || _effort < 0.01 )
                 {
                    fc::usleep( fc::microseconds( 1000*10 ) );
                    continue;
                 }
                 auto start = fc::time_point::now();

                 block_header tmp = _current_block;
                 tmp.noncea = 0;
                 tmp.nonceb = 0;
                 auto tmp_id = tmp.id();
                 auto seed = fc::sha256::hash( (char*)&tmp_id, sizeof(tmp_id) );
                 auto pairs = momentum_search( seed );
                 for( auto collision : pairs )
                 {
                    tmp.noncea = collision.first;
                    tmp.nonceb = collision.second;
                    if( (tmp.get_difficulty() * _miner_votes)/_min_votes  >= _target )
                    {
                       if( _callback )
                       {
                          auto cb = _callback; 
                          _main_thread->async( [cb,tmp](){cb( tmp );} );
                       }
                       continue;
                    }
                 }

                 // search space...
                 
                 auto end   = fc::time_point::now();

                 // wait while checking for cancel...
                 if( _effort < 1.0 )
                 {
                    auto calc_time = (end-start).count();
                    auto wait_time = ((1-_effort)/_effort) * calc_time;

                    auto wait_until = end + fc::microseconds(wait_time);
                    if( wait_until > fc::time_point::now() && !_mining_loop_complete.canceled() )
                    {
                       fc::usleep( fc::microseconds( 1000*10 ) );
                    }
                 }
                 else
                 {
                    fc::usleep( fc::microseconds(1000) );
                 }
              }
           }
     };
  }

  block_miner::block_miner()
  :my( new detail::block_miner_impl() )
  {
     my->_main_thread = &fc::thread::current();
     my->_mining_thread.set_name( "mining" );
     my->_mining_loop_complete = my->_mining_thread.async( [=](){ my->mining_loop(); } );
  }

  block_miner::~block_miner()
  {
     my->_mining_loop_complete.cancel();
     try {
        my->_mining_loop_complete.wait();
     } 
     catch ( const fc::exception& e )
     {
        wlog( "${e}", ( "e", e.to_detail_string() ) );
     }
  }

  void block_miner::set_block( const block_header& header, 
                               uint64_t target_difficulty, 
                               uint64_t miner_votes, uint64_t min_votes )
  {
     my->_current_block = header;
     my->_target        = target_difficulty;
     my->_miner_votes   = miner_votes;
     my->_min_votes     = min_votes;
  }

  void block_miner::set_effort( float effort )
  {
     my->_effort = effort;
  }
  void block_miner::set_callback( const callback& cb )
  {
     my->_callback = cb;
  }


  
} } // bts::blockchain
