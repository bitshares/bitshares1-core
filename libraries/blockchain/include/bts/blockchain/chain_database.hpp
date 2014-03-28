#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/pow_validator.hpp>

namespace fc 
{
   class path;
};

namespace bts { namespace blockchain {

    namespace detail  { class chain_database_impl; }

    /**
     *  @class chain_database 
     *  @ingroup blockchain
     *
     *  The chain_database provides the generic implementation of basic 
     *  blockchain semantics which requires that blocks can be pushed,
     *  popped and that all transactions are validated.  
     *
     *  There can be many variations on the chain_database each of which
     *  implements custom logic for handling different transactions.
     *
     *  This database only stores valid blocks and applied transactions,
     *  it does not store invalid/orphaned blocks and transactions which
     *  are maintained in a separate database 
     */
    class chain_database 
    {
       protected:
          /**
           *  This method should perform the validation step of making sure that the block and
           *  all associated  determinsitc transactions can be applied.  It should throw an
           *  exception of the validation fails.
           */
          virtual void validate( const trx_block& blk, const signed_transactions& determinsitc_trxs );

          /** 
           *  Called after a block has been validated and appends
           *  it to the block chain storing all relevant transactions.
           */
          virtual void store( const trx_block& blk, const signed_transactions& determinsitc_trxs );


       public:
          chain_database();
          virtual ~chain_database();

          /**
           *  There are many kinds of deterministic transactions that various blockchains may require
           *  such as automatic inactivity fees, lottery winners, and market making.   This method
           *  can be overloaded to 
           */
          virtual signed_transactions generate_determinsitic_transactions();

          void evaluate_transaction( const signed_transaction& trx );


          //@{
          /**
           *  The signing authority is a key that signs every block
           *  once they have successfully pushed it to their chain.  
           *
           *  The purpose of the signing authority is to make sure that
           *  there are no chain forks until the consensus algorithm can
           *  be reached.  
           */
          void                        set_trustee( const address& addr );
          address                     get_trustee()const;
          //@}

          /**
           * When testing the chain there are different POW validation checks, so
           * this must be set by the creator of the chain.
           */
          void set_pow_validator( const pow_validator_ptr& v );
          pow_validator_ptr get_pow_validator()const;

          void set_transaction_validator( const transaction_validator_ptr& v );
          transaction_validator_ptr get_transaction_validator()const;

          void open( const fc::path& dir, bool create = true );
          void close();

          const signed_block_header&  get_head_block()const;
          uint64_t                    total_shares()const;
          uint32_t                    head_block_num()const;
          block_id_type               head_block_id()const;
          uint64_t                    get_stake(); // head - 1 
          asset                       get_fee_rate()const;



         trx_num    fetch_trx_num( const uint160& trx_id );
         meta_trx   fetch_trx( const trx_num& t );

         signed_transaction          fetch_transaction( const transaction_id_type& trx_id );
         std::vector<meta_trx_input> fetch_inputs( const std::vector<trx_input>& inputs, uint32_t head = trx_num::invalid_block_num );

         trx_output fetch_output(const output_reference& ref);

         uint32_t                   fetch_block_num( const block_id_type& block_id );
         signed_block_header        fetch_block( uint32_t block_num );
         digest_block               fetch_digest_block( uint32_t block_num );
         trx_block                  fetch_trx_block( uint32_t block_num );

         /**
          *  Validates the block and then pushes it into the database.
          *
          *  Attempts to append block b to the block chain with the given trxs.
          */
         void push_block( const trx_block& b );

         /**
          *  Removes the top block from the stack and marks all spent outputs as 
          *  unspent.
          */
         virtual trx_block pop_block();

       private:
         void   store_trx( const signed_transaction& trx, const trx_num& t );
         std::unique_ptr<detail::chain_database_impl> my;          
    }; // chain_database

    typedef std::shared_ptr<chain_database> chain_database_ptr;

}  } // bts::blockchain

FC_REFLECT( bts::blockchain::trx_num,  (block_num)(trx_idx) );

