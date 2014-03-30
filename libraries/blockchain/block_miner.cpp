#include <bts/blockchain/block_miner.hpp>
#include <bts/blockchain/momentum.hpp>
#include <fc/thread/thread.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {

  namespace detail 
  { 
     class block_miner_impl
     {
        public:
           block_miner_impl()
           :_miner_votes(0),_min_votes(1),_effort(0){}

           block_miner::callback _callback;
           fc::thread*           _main_thread;
           fc::thread            _mining_thread;
           uint64_t              _miner_votes;
           uint64_t              _min_votes;
           block_header          _current_block;
           fc::future<void>      _mining_loop_complete;
           float                 _effort;
           block_header          _prev_header;

           void mining_loop()
           {
              while( !_mining_loop_complete.canceled() )
              {
                try {
                    if( _current_block.prev == block_id_type() || !_callback || _effort < 0.01 || _prev_header.next_difficulty == 0 )
                    {
                       ilog( "${current.prev}  _effort ${effort}  prev_header: ${prev_header}", 
                             ("current.prev",_current_block.prev)("effort",_effort)("prev_header",_prev_header) );
                       fc::usleep( fc::microseconds( 1000*1000 ) );
                       continue;
                    }
                    auto start = fc::time_point::now();
                   
                    block_header tmp = _current_block;
                    tmp.timestamp = fc::time_point::now();
                    auto next_diff = _prev_header.next_difficulty * 300*1000000ll / (tmp.timestamp - _prev_header.timestamp).count();
                    tmp.next_difficulty = (_prev_header.next_difficulty * 24 + next_diff ) / 25;
                   
                    tmp.noncea = 0;
                    tmp.nonceb = 0;
                    auto tmp_id = tmp.id();
                    auto seed = fc::sha256::hash( (char*)&tmp_id, sizeof(tmp_id) );
                    auto pairs = momentum_search( seed );
                    for( auto collision : pairs )
                    {
                       tmp.noncea = collision.first;
                       tmp.nonceb = collision.second;
                       FC_ASSERT( _min_votes > 0 );
                       FC_ASSERT( _prev_header.next_difficulty > 0 );
                       ilog( "difficlty ${d}  target ${t}  tmp.get_difficulty ${dd}  mv ${mv} min: ${min}  block:\n${block}", ("min",_min_votes)("mv",_miner_votes)("dd",tmp.get_difficulty())
                                                                                             ("d",(tmp.get_difficulty() * _miner_votes)/_min_votes)("t",_prev_header.next_difficulty)("block",_current_block) );
                       if( (tmp.get_difficulty() * _miner_votes)/_min_votes  >= _prev_header.next_difficulty )
                       {
                          if( _callback )
                          {
                             auto cb = _callback; 
                             _main_thread->async( [cb,tmp](){cb( tmp );} );
                          }
                          _effort = 0;
                          break;
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
                          ilog( "." );
                          fc::usleep( fc::microseconds( 1000*100 ) );
                       }
                    }
                    else
                    {
                       ilog( "." );
                       fc::usleep( fc::microseconds(1000*10) );
                    }
                }
                catch ( const fc::exception& e )
                {
                   wlog( "${e}", ("e",e.to_detail_string() ));
                }
              } // while 
           } /// mining_loop
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

  void block_miner::set_block( const block_header& header, const block_header& prev_header,
                               uint64_t miner_votes, uint64_t min_votes )
  {
     FC_ASSERT( min_votes > 0 );
     my->_current_block = header;
     my->_prev_header   = prev_header;
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
