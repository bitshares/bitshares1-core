
#pragma once
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/chain_database.hpp>

#include <bts/lotto/lotto_transaction_validator.hpp>

namespace bts { namespace lotto {

namespace detail  { class lotto_db_impl; }

struct drawing_record
{
   drawing_record()
   :total_jackpot(0),total_paid(0){}

   uint64_t total_jackpot;
   uint64_t total_paid;
};
struct block_summary
{
   block_summary()
   :ticket_sales(0),amount_won(0){}

   uint64_t ticket_sales;
   uint64_t amount_won;
};

class lotto_db : public bts::blockchain::chain_database
{
    public:
        lotto_db();
        ~lotto_db();
    
        void             open( const fc::path& dir, bool create );
        void             close();

        uint64_t get_jackpot_for_ticket( uint64_t ticket_block_num, 
                                         fc::sha256& winning_number, 
                                         uint64_t& global_odds );

        /**
         * Performs global validation of a block to make sure that no two transactions conflict. In
         * the case of the lotto only one transaction can claim the jackpot.
         */
        virtual void validate( const trx_block& blk, const signed_transactions& determinsitc_trxs );

        /** 
         *  Called after a block has been validated and appends
         *  it to the block chain storing all relevant transactions and updating the
         *  winning database.
         */
        virtual void store( const trx_block& blk, const signed_transactions& determinsitc_trxs );

        /**
         * When a block is popped from the chain, this method implements the necessary code
         * to revert the blockchain database to the proper state.
         */
        virtual trx_block pop_block();
    private:
         std::unique_ptr<detail::lotto_db_impl> my;

};


}} // bts::lotto


FC_REFLECT( bts::lotto::drawwing_record, (total_jackpot)(total_paid) )
FC_REFLECT( bts::lotto::block_summary, (ticket_sales)(amount_won) )
