
#include <bts/db/level_map.hpp>
#include <bts/lotto/lotto_db.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace lotto {

    namespace detail
    {
        class lotto_db_impl
        {
            public:
                lotto_db_impl(){}
                // map drawning number to drawing record
                bts::db::level_map<uint32_t, drawing_record>  _drawing2record;
                bts::db::level_map<uint32_t, block_summary>   _block2summary;

        };
    }

    lotto_db::lotto_db()
    :my( new detail::lotto_db_impl() )
    {
        set_transaction_validator( std::make_shared<lotto_transaction_validator>(this) );
    }

    lotto_db::~lotto_db()
    {
    }

    void lotto_db::open( const fc::path& dir, bool create )
    {
        try {
            chain_database::open( dir, create );
            my->drawing2record.open( dir / "drawing2record", create );
            my->block2summary.open( dir / "block2summary", create );
        } FC_RETHROW_EXCEPTIONS( warn, "Error loading domain database ${dir}", ("dir", dir)("create", create) );
    }

    void lotto_db::close()
    {
        my->name2record.close();
    }

    uint64_t lotto_db::get_jackpot_for_ticket( uint64_t ticket_block_num,
                                     fc::sha256& winning_number,
                                     uint64_t& global_odds )
    {
    }

    /**
     * Performs global validation of a block to make sure that no two transactions conflict. In
     * the case of the lotto only one transaction can claim the jackpot.
     */
    void lotto_db::validate( const trx_block& blk, const signed_transactions& deterministic_trxs )
    {
    }

    /**
     *  Called after a block has been validated and appends
     *  it to the block chain storing all relevant transactions and updating the
     *  winning database.
     */
    void lotto_db::store( const trx_block& blk, const signed_transactions& deterministic_trxs )
    {
    }

    /**
     * When a block is popped from the chain, this method implements the necessary code
     * to revert the blockchain database to the proper state.
     */
    trx_block lotto_db::pop_block()
    {
       auto blk = chain_database::pop_block();
       // TODO: remove block summary from DB
       FC_ASSERT( !"Not Implemented" );
       return blk;
    }

}} // bts::lotto
