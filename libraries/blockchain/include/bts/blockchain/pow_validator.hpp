#pragma once
#include <bts/blockchain/block.hpp>

namespace bts { namespace blockchain {
   class chain_database;

   class pow_validator
   {
      public:
        pow_validator( chain_database* db = nullptr ):_db(db){}
        virtual ~pow_validator(){}
        virtual bool              validate_work( const block_header& header, uint64_t valid_miner_votes, uint64_t min_votes );
        virtual fc::time_point    get_time()const { return fc::time_point::now(); }

        chain_database* _db;
   };

   class sim_pow_validator : public pow_validator
   {
      public:
        sim_pow_validator( const fc::time_point& start_time ):_time(start_time){}

        virtual bool              validate_work( const block_header& header, uint64_t valid_miner_votes, uint64_t min_votes ) { return true; }
        virtual void              skip_time( const fc::microseconds& dt ) { _time += dt; }
        virtual fc::time_point    get_time()const { return _time; }

      private:
        fc::time_point   _time;
   };

   typedef std::shared_ptr<pow_validator> pow_validator_ptr;

} } // bts::blockchain
