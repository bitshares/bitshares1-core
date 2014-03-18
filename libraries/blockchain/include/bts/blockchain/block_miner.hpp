#pragma once
#include <bts/blockchain/block.hpp>

namespace bts { namespace blockchain {

  namespace detail { class block_miner_impl; }
  
  /**
   *  @class block_miner;
   *  @brief Mines blocks in a background thread.
   */
  class block_miner 
  {
     public:
        typedef std::function<void( const block_header& h )>  callback;
        block_miner();
        ~block_miner();

        void set_block( const block_header& header, uint64_t target_difficulty, uint64_t miner_votes, uint64_t available_votes );
        void set_effort( float effort );
        void set_callback( const callback& cb );

     private:
        std::unique_ptr<detail::block_miner_impl> my;
  };

} }
