#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/pow_validator.hpp>

namespace fc
{
   class path;
};

namespace bts {
   /**
    *  @brief all types and data associated with the blockchain data structure
    */
   namespace blockchain {

    namespace detail  { class chain_database_impl; }
    uint64_t to_bips( uint64_t shares, uint64_t total_shares );

    struct name_record
    {
       name_record()
       :delegate_id(0),votes_for(0),votes_against(0){}
       name_record( const claim_name_output& o )
          :delegate_id(o.delegate_id),name(o.name),data(o.data),owner(o.owner),votes_for(0),votes_against(0){}

       name_record( uint32_t id, std::string n, const fc::ecc::public_key& k )
       :delegate_id(id),name(n),owner(k),votes_for(0),votes_against(0){}

       int64_t total_votes()const { return votes_for - votes_against; }

       uint32_t             delegate_id;
       std::string          name;
       std::string          data;
       fc::ecc::public_key  owner;
       int64_t              votes_for;
       int64_t              votes_against;
    };

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
           *  all associated deterministic transactions can be applied. It should throw an
           *  exception of the validation fails.
           */
          virtual block_evaluation_state_ptr validate( const trx_block& blk, const signed_transactions& deterministic_trxs );

          /**
           *  Called after a block has been validated and appends
           *  it to the block chain storing all relevant transactions.
           */
          virtual void store( const trx_block& blk, const signed_transactions& deterministic_trxs, const block_evaluation_state_ptr& state );


       public:
          chain_database();
          virtual ~chain_database();

          /**
           *  There are many kinds of deterministic transactions that various blockchains may require
           *  such as automatic inactivity fees, lottery winners, and market making.   This method
           *  can be overloaded to
           */
          virtual signed_transactions   generate_deterministic_transactions();

          void                          evaluate_transaction( const signed_transaction& trx );

          fc::optional<name_record>     lookup_name( const std::string& name );
          fc::optional<name_record>     lookup_delegate( uint32_t del );

          /**
           *  @param count - the number of delegates to return
           *
           *  @return the top *count* delegates by vote, sorted in descending order.
           */
          std::vector<name_record>      get_delegates( uint32_t count = BTS_BLOCKCHAIN_NUM_DELEGATES );

          /**
           * Returns up to count names after first when sorted alphabetically
           */
          std::vector<name_record>      get_names( const std::string& first, uint32_t count = BTS_BLOCKCHAIN_NUM_DELEGATES );

          //@{
          /**
           *  The signing authority is a key that signs every block
           *  once they have successfully pushed it to their chain.
           *
           *  The purpose of the signing authority is to make sure that
           *  there are no chain forks until the consensus algorithm can
           *  be reached.
           */
          void                          set_trustee( const address& addr );
          address                       get_trustee()const;
          //@}

          /**
           * When testing the chain there are different POW validation checks, so
           * this must be set by the creator of the chain.
           */
          void                          set_pow_validator( const pow_validator_ptr& v );
          pow_validator_ptr             get_pow_validator()const;

          void                          set_transaction_validator( const transaction_validator_ptr& v );
          transaction_validator_ptr     get_transaction_validator()const;

          virtual void                  open( const fc::path& dir, bool create = true );
          virtual void                  close();

          const signed_block_header&    get_head_block()const;
          uint64_t                      total_shares()const;
          uint32_t                      head_block_num()const;
          block_id_type                 head_block_id()const;
          uint64_t                      get_stake(); // head - 1

          /** return the fee rate in shares */
          uint64_t                      get_fee_rate()const;
          uint32_t                      get_new_delegate_id()const;

          uint32_t                      get_output_age( const output_reference& output_ref );

          trx_num                       fetch_trx_num( const transaction_id_type& trx_id );
          meta_trx                      fetch_trx( const trx_num& t );

          signed_transaction            fetch_transaction( const transaction_id_type& trx_id );
          std::vector<meta_trx_input>   fetch_inputs( const std::vector<trx_input>& inputs, uint32_t head = trx_num::invalid_block_num );

          trx_output                    fetch_output(const output_reference& ref);

          uint32_t                      fetch_block_num( const block_id_type& block_id );
          signed_block_header           fetch_block( uint32_t block_num );
          digest_block                  fetch_digest_block( uint32_t block_num );
          trx_block                     fetch_trx_block( uint32_t block_num );

          /**
           *  Validates the block and then pushes it into the database.
           *
           *  Attempts to append block b to the block chain with the given trxs.
           */
          void                          push_block( const trx_block& b );

          /**
           *  Removes the top block from the stack and marks all spent outputs as
           *  unspent.
           */
          virtual trx_block             pop_block();

       private:
          void                          store_trx( const signed_transaction& trx, const trx_num& t );

          std::unique_ptr<detail::chain_database_impl> my;
    }; // chain_database

    typedef std::shared_ptr<chain_database> chain_database_ptr;

}  } // bts::blockchain

FC_REFLECT( bts::blockchain::trx_num,  (block_num)(trx_idx) );
FC_REFLECT( bts::blockchain::name_record, (delegate_id)(name)(data)(owner)(votes_for)(votes_against) )

