#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/asset.hpp>
#include <leveldb/db.h>
#include <bts/db/level_pod_map.hpp>
#include <bts/db/level_map.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>
#include <fc/interprocess/mmap_struct.hpp>

#include <fc/filesystem.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>

#include <algorithm>
#include <sstream>

namespace fc {
  template<> struct get_typename<std::vector<uint160>>    { static const char* name()  { return "std::vector<uint160>";  } };
} // namespace fc

struct trx_stat
{
   uint16_t trx_idx;
   bts::blockchain::transaction_summary eval;
};
// sort with highest fees first
bool operator < ( const trx_stat& a, const trx_stat& b )
{
  return a.eval.fees > b.eval.fees;
}
FC_REFLECT( trx_stat, (trx_idx)(eval) )

namespace bts { namespace blockchain {
    namespace ldb = leveldb;
    namespace detail  
    { 
      
      // TODO: .01 BTC update private members to use _member naming convention
      class chain_database_impl
      {
         public:
            chain_database_impl()
            {
               _pow_validator = std::make_shared<pow_validator>();
            }

            //std::unique_ptr<ldb::DB> blk_id2num;  // maps blocks to unique IDs
            bts::db::level_map<block_id_type,uint32_t>          blk_id2num;
            bts::db::level_map<uint160,trx_num>                 trx_id2num;
            bts::db::level_map<trx_num,meta_trx>                meta_trxs;
            bts::db::level_map<uint32_t,block_header>           blocks;
            bts::db::level_map<uint32_t,std::vector<uint160> >  block_trxs; 

            pow_validator_ptr                                   _pow_validator;
            transaction_validator_ptr                           _trx_validator;


            /** cache this information because it is required in many calculations  */
            trx_block                                           head_block;
            block_id_type                                       head_block_id;

            void mark_spent( const output_reference& o, const trx_num& intrx, uint16_t in )
            {
               auto tid    = trx_id2num.fetch( o.trx_hash );
               meta_trx   mtrx   = meta_trxs.fetch( tid );
               FC_ASSERT( mtrx.meta_outputs.size() > o.output_idx );

               mtrx.meta_outputs[o.output_idx].trx_id    = intrx;
               mtrx.meta_outputs[o.output_idx].input_num = in;

               meta_trxs.store( tid, mtrx );
            }

            trx_output get_output( const output_reference& ref )
            { try {
               auto tid    = trx_id2num.fetch( ref.trx_hash );
               meta_trx   mtrx   = meta_trxs.fetch( tid );
               FC_ASSERT( mtrx.outputs.size() > ref.output_idx );
               return mtrx.outputs[ref.output_idx];
            } FC_RETHROW_EXCEPTIONS( warn, "", ("ref",ref) ) }
            
            /**
             *   Stores a transaction and updates the spent status of all 
             *   outputs doing one last check to make sure they are unspent.
             */
            void store( const signed_transaction& t, const trx_num& tn )
            {
               ilog( "trxid: ${id}   ${tn}\n\n  ${trx}\n\n", ("id",t.id())("tn",tn)("trx",t) );

               trx_id2num.store( t.id(), tn ); 
               meta_trxs.store( tn, meta_trx(t) );

               for( uint16_t i = 0; i < t.inputs.size(); ++i )
               {
                  mark_spent( t.inputs[i].output_ref, tn, i ); 
               }
            }

            void store( const trx_block& b )
            {
                std::vector<uint160> trxs_ids;
                for( uint16_t t = 0; t < b.trxs.size(); ++t )
                {
                   store( b.trxs[t], trx_num( b.block_num, t) );
                   trxs_ids.push_back( b.trxs[t].id() );
                }
                head_block    = b;
                head_block_id = b.id();

                blocks.store( b.block_num, b );
                block_trxs.store( b.block_num, trxs_ids );

                blk_id2num.store( b.id(), b.block_num );
            }
      };
    }

     chain_database::chain_database()
     :my( new detail::chain_database_impl() )
     {
         my->_trx_validator = std::make_shared<transaction_validator>(this);
     }

     chain_database::~chain_database()
     {
     }

     void chain_database::open( const fc::path& dir, bool create )
     {
       try {
         if( !fc::exists( dir ) )
         {
              if( !create )
              {
                 FC_THROW_EXCEPTION( file_not_found_exception, 
                     "Unable to open blockchain database ${dir}", ("dir",dir) );
              }
              fc::create_directories( dir );
         }
         my->blk_id2num.open( dir / "blk_id2num", create );
         my->trx_id2num.open( dir / "trx_id2num", create );
         my->meta_trxs.open(  dir / "meta_trxs",  create );
         my->blocks.open(     dir / "blocks",     create );
         my->block_trxs.open( dir / "block_trxs", create );

         
         // read the last block from the DB
         my->blocks.last( my->head_block.block_num, my->head_block );
         if( my->head_block.block_num != uint32_t(-1) )
         {
            my->head_block_id = my->head_block.id();
         }

       } FC_RETHROW_EXCEPTIONS( warn, "error loading blockchain database ${dir}", ("dir",dir)("create",create) );
     }

     void chain_database::close()
     {
        my->blk_id2num.close();
        my->trx_id2num.close();
        my->blocks.close();
        my->block_trxs.close();
        my->meta_trxs.close();
     }

    uint32_t chain_database::head_block_num()const
    {
       return my->head_block.block_num;
    }
    block_id_type chain_database::head_block_id()const
    {
       return my->head_block.id();
    }


    /**
     *  @pre trx must pass evaluate_signed_transaction() without exception
     *  @pre block_num must be a valid block 
     *
     *  @param block_num - the number of the block that contains this trx.
     *
     *  @return the index / trx number that was assigned to trx as part of storing it.
    void  chain_database::store_trx( const signed_transaction& trx, const trx_num& trx_idx )
    {
       try {
         my->trx_id2num.store( trx.id(), trx_idx );
         
         meta_trx mt(trx);
         mt.meta_outputs.resize( trx.outputs.size() );
         my->meta_trxs.store( trx_idx, mt );

       } FC_RETHROW_EXCEPTIONS( warn, 
          "an error occured while trying to store the transaction", 
          ("trx",trx) );
    }
     */

    trx_num    chain_database::fetch_trx_num( const uint160& trx_id )
    { try {
       return my->trx_id2num.fetch(trx_id);
    } FC_RETHROW_EXCEPTIONS( warn, "trx_id ${trx_id}", ("trx_id",trx_id) ) }

    meta_trx    chain_database::fetch_trx( const trx_num& trx_id )
    { try {
       return my->meta_trxs.fetch( trx_id );
    } FC_RETHROW_EXCEPTIONS( warn, "trx_id ${trx_id}", ("trx_id",trx_id) ) }

    uint32_t    chain_database::fetch_block_num( const block_id_type& block_id )
    { try {
       return my->blk_id2num.fetch( block_id ); 
    } FC_RETHROW_EXCEPTIONS( warn, "block id: ${block_id}", ("block_id",block_id) ) }

    block_header chain_database::fetch_block( uint32_t block_num )
    {
       return my->blocks.fetch(block_num);
    }

    digest_block  chain_database::fetch_digest_block( uint32_t block_num )
    { try {
       digest_block fb = my->blocks.fetch(block_num);
       fb.trx_ids = my->block_trxs.fetch( block_num );
       return fb;
    } FC_RETHROW_EXCEPTIONS( warn, "block ${block}", ("block",block_num) ) }

    trx_block  chain_database::fetch_trx_block( uint32_t block_num )
    { try {
       trx_block fb = my->blocks.fetch(block_num);
       auto trx_ids = my->block_trxs.fetch( block_num );
       for( uint32_t i = 0; i < trx_ids.size(); ++i )
       {
          auto trx_num = fetch_trx_num(trx_ids[i]);
          fb.trxs.push_back( fetch_trx( trx_num ) );
       }
       // TODO: fetch each trx and add it to the trx block
       //fb.trx_ids = my->block_trxs.fetch( block_num );
       return fb;
    } FC_RETHROW_EXCEPTIONS( warn, "block ${block}", ("block",block_num) ) }

    signed_transaction chain_database::fetch_transaction( const transaction_id_type& id )
    { try {
          auto trx_num = fetch_trx_num(id);
          return fetch_trx( trx_num );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("id",id) ) }

    trx_output chain_database::fetch_output(const output_reference& ref) 
    {
        auto trx = fetch_transaction(ref.trx_hash);
        return trx.outputs[ref.output_idx];
    }

    std::vector<meta_trx_input> chain_database::fetch_inputs( const std::vector<trx_input>& inputs, uint32_t head )
    {
       try
       {
          if( head == uint32_t(-1) )
          {
            head = head_block_num();
          }

          std::vector<meta_trx_input> rtn;
          rtn.reserve( inputs.size() );
          for( uint32_t i = 0; i < inputs.size(); ++i )
          {
            try {
             trx_num tn   = fetch_trx_num( inputs[i].output_ref.trx_hash );
             meta_trx trx = fetch_trx( tn );
             
             if( inputs[i].output_ref.output_idx >= trx.meta_outputs.size() )
             {
                FC_THROW_EXCEPTION( exception, "Input ${i} references invalid output from transaction ${trx}",
                                    ("i",inputs[i])("trx", trx) );
             }
             if( inputs[i].output_ref.output_idx >= trx.outputs.size() )
             {
                FC_THROW_EXCEPTION( exception, "Input ${i} references invalid output from transaction ${t}",
                                    ("i",inputs[i])("o", trx) );
             }

             meta_trx_input metin;
             metin.source       = tn;
             metin.output_num   = inputs[i].output_ref.output_idx;
             metin.output       = trx.outputs[metin.output_num];
             metin.meta_output  = trx.meta_outputs[metin.output_num];
             rtn.push_back( metin );

            } FC_RETHROW_EXCEPTIONS( warn, "error fetching input [${i}] ${in}", ("i",i)("in", inputs[i]) );
          }
          return rtn;
       } FC_RETHROW_EXCEPTIONS( warn, "error fetching transaction inputs", ("inputs", inputs) );
    }


    /**
     *  Validates that trx could be included in a future block, that
     *  all inputs are unspent, that it is valid for the current time,
     *  and that all inputs have proper signatures and input data.
     *
     *  @return any trx fees that would be paid if this trx were included
     *          in the next block.
     *
     *  @throw exception if trx can not be applied to the current chain state.
    trx_eval chain_database::evaluate_signed_transaction( const signed_transaction& trx, bool ignore_fees  )       
    {
       try {
           FC_ASSERT( trx.inputs.size() || trx.outputs.size()    );
           FC_ASSERT( trx.valid_until > my->head_block.timestamp );

           trx_validation_state vstate( trx, this ); 
           vstate.prev_block_id1 = get_stake();
           vstate.prev_block_id2 = get_stake2();
           vstate.validate();

           trx_eval e;
           if( my->head_block_id != block_id_type() )
           {
              // all transactions must pay at least some fee 
              if( vstate.balance_sheet[asset::bts].out >= vstate.balance_sheet[asset::bts].in )
              {
                
                 FC_ASSERT( vstate.balance_sheet[asset::bts].out <= vstate.balance_sheet[asset::bts].in, 
                            "All transactions must pay some fee",
                 ("out", vstate.balance_sheet[asset::bts].out)("in",vstate.balance_sheet[asset::bts].in )
                            );
              }
              else
              {
                 e.fees = vstate.balance_sheet[asset::bts].in - vstate.balance_sheet[asset::bts].out;
                 if( !ignore_fees )
                 {
                    FC_ASSERT( e.fees.get_rounded_amount() >= (get_fee_rate() * trx.size()).get_rounded_amount() );
                 }
              }
           }
           e.total_spent += vstate.balance_sheet[asset::bts].in.get_rounded_amount() + vstate.balance_sheet[asset::bts].collat_in.get_rounded_amount();
           e.coindays_destroyed = vstate.total_cdd;
           e.invalid_coindays_destroyed = vstate.uncounted_cdd;
           return e;
       } FC_RETHROW_EXCEPTIONS( warn, "error evaluating transaction ${t}", ("t", trx) );
    }



    trx_eval chain_database::evaluate_signed_transactions( const std::vector<signed_transaction>& trxs, uint64_t ignore_first_n_fees )
    {
      try {
        trx_eval total_eval;
        for( uint32_t i = 0; i < trxs.size(); ++i )
        {
            // ignore fees for the market trxs and for the mining transaction... assuming there is a mining trx??
            if( i < ignore_first_n_fees )
            {
               total_eval += evaluate_signed_transaction( trxs[i], true, true );
            }
            bts::address mining_addr;
            if( i == trxs.size() - 1 ) // last trx..
            {
               if( trxs.back().inputs.size() == 0 )
               {
                  if( trxs.size() > 1 ) 
                  {
                     if( trxs[i-1].outputs.size() == 1 ) // mining trx can only have 1 output
                     {
                        if( trxs[i-1].outputs[0].claim_func == claim_by_signature ) // mining trx must be claim by sig
                        {
                           mining_addr =  trxs[i-1].outputs[0].as<claim_by_signature_output>().owner;
                           FC_ASSERT( trxs.back().outputs.size() == 1 ); // only allowed 1 output
                           FC_ASSERT( trxs.back().outputs.back().as<claim_by_signature_output>().owner == mining_addr ); // must match

                           auto prev_eval = evaluate_signed_transaction( trxs[i-1], true );

                           auto rew = (total_eval.fees.get_rounded_amount() * prev_eval.coindays_destroyed )/
                                                total_eval.coindays_destroyed;
                           asset mining_reward(rew, asset::bts); 
                           // calculate mining reward... 
                           FC_ASSERT( trxs.back().outputs.back().amount == mining_reward );
                        }
                     }
                  }
               }
               if( mining_addr == bts::address() ) // process like normal
               {
                  total_eval += evaluate_signed_transaction( trxs[i], false );
               }
            }
            else 
            {
               total_eval += evaluate_signed_transaction( trxs[i], 
                                    (i == trxs.size()-1) || (i < ignore_first_n_fees) );
            }
        }
        ilog( "summary: ${totals}", ("totals",total_eval) );
        return total_eval;
      } FC_RETHROW_EXCEPTIONS( debug, "" );
    }
    */

    void validate_unique_inputs( const std::vector<signed_transaction>& trxs )
    {
       std::unordered_set<output_reference> ref_outs;
       for( auto itr = trxs.begin(); itr != trxs.end(); ++itr )
       {
          for( auto in = itr->inputs.begin(); in != itr->inputs.end(); ++in )
          {
             if( !ref_outs.insert( in->output_ref ).second )
             {
                FC_THROW_EXCEPTION( exception, "duplicate input detected",
                                            ("in", *in )("trx",*itr)  );
             }
          }
       }
    }
    
    /**
     *  Attempts to append block b to the block chain with the given trxs.
     */
    void chain_database::push_block( const trx_block& b )
    {
      try {
        FC_ASSERT( b.version      == 0                                                         );
        FC_ASSERT( b.trxs.size()  > 0                                                          );
        FC_ASSERT( b.block_num    == head_block_num() + 1                                      );
        FC_ASSERT( b.prev         == my->head_block_id                                         );
        FC_ASSERT( b.trx_mroot    == b.calculate_merkle_root()                                 );
        /// time stamps from the future are not allowed
        FC_ASSERT( b.next_fee     == b.calculate_next_fee( get_fee_rate().get_rounded_amount(), b.block_size() ), "",
                   ("b.next_fee",b.next_fee)("b.calculate_next_fee", b.calculate_next_fee( get_fee_rate().get_rounded_amount(), b.block_size()))
                   ("get_fee_rate",get_fee_rate().get_rounded_amount())("b.size",b.block_size()) 
                   );

        if( b.block_num >= 1 )
        {
           FC_ASSERT( b.timestamp    <= (my->_pow_validator->get_time() + fc::seconds(60)), "",
                      ("b.timestamp", b.timestamp)("future",my->_pow_validator->get_time()+ fc::seconds(60)));
           
           FC_ASSERT( b.timestamp    > fc::time_point(my->head_block.timestamp) + fc::seconds(30) );
           my->_pow_validator->validate_work( b );
           /*
           FC_ASSERT( b.get_difficulty() >= b.get_required_difficulty( 
                                                 my->head_block.next_difficulty,
                                                 my->head_block.avail_coindays ), "",
                      ("required_difficulty",b.get_required_difficulty( my->head_block.next_difficulty, my->head_block.avail_coindays )  )
                      ("block_difficulty", b.get_difficulty() ) );
                      */
        }

        //validate_issuance( b, my->head_block /*aka new prev*/ );
        validate_unique_inputs( b.trxs );

        transaction_summary summary;
        for( auto strx : b.trxs )
        {
            summary += my->_trx_validator->evaluate( strx ); 
        }

        // TODO: validate that the header records the proper fees
        store( b );
        
      } FC_RETHROW_EXCEPTIONS( warn, "unable to push block", ("b", b) );
    } // chain_database::push_block

    void chain_database::store( const trx_block& blk )
    {
        my->store( blk ); 
    }

    /**
     *  Removes the top block from the stack and marks all spent outputs as 
     *  unspent.
     */
    trx_block chain_database::pop_block() 
    {
       FC_ASSERT( !"TODO: implement pop_block" );
    }



    /**
     *  First step to creating a new block is to take all canidate transactions and 
     *  sort them by fees and filter out transactions that are not valid.  Then
     *  filter out incompatible transactions (those that share the same inputs).
     */
    trx_block  chain_database::generate_next_block( const std::vector<signed_transaction>& in_trxs )
    {
      try {
         trx_block result;
         std::vector<trx_stat>  stats;
         stats.reserve(in_trxs.size());

         for( uint32_t i = 0; i < in_trxs.size(); ++i )
         {
            try {
                trx_stat s;
                s.eval = my->_trx_validator->evaluate( in_trxs[i] ); //evaluate_signed_transaction( in_trxs[i] );
                ilog( "eval: ${eval}", ("eval",s.eval) );

               // TODO: enforce fees
                if( s.eval.fees < (get_fee_rate() * in_trxs[i].size()).get_rounded_amount() )
                {
                  wlog( "ignoring transaction ${trx} because it doesn't pay minimum fee ${f}\n\n state: ${s}", 
                        ("trx",in_trxs[i])("s",s.eval)("f", get_fee_rate()*in_trxs[i].size()) );
                  continue;
                }
                s.trx_idx = i;
                stats.push_back( s );
            } 
            catch ( const fc::exception& e )
            {
               wlog( "unable to use trx ${t}\n ${e}", ("t", in_trxs[i] )("e",e.to_detail_string()) );
            }
         }
         std::sort( stats.begin(), stats.end() ); 
         std::unordered_set<output_reference> consumed_outputs;

         for( size_t i = 0; i < stats.size(); ++i )
         {
            const signed_transaction& trx = in_trxs[stats[i].trx_idx]; 
            for( size_t in = 0; in < trx.inputs.size(); ++in )
            {
               if( !consumed_outputs.insert( trx.inputs[in].output_ref ).second )
               {
                    stats[i].trx_idx = uint16_t(-1); // mark it to be skipped, input conflict
                    break; 
               }
            }
            result.trxs.push_back(trx);
         }


         result.block_num = head_block_num() + 1;
         result.prev      = head_block_id();
         result.trx_mroot = result.calculate_merkle_root();
         result.next_fee  = result.calculate_next_fee( get_fee_rate().get_rounded_amount(), result.block_size() );
         result.timestamp = my->_pow_validator->get_time();

         return result;
         #if 0
         std::vector<signed_transaction> trxs;// = match_orders();
         size_t num_orders = trxs.size();
         ilog( "." );
         for( uint32_t i = 0; i < in_trxs.size(); ++i )
         {
            ilog( "trx: ${t} signed by ${s}", ( "t",in_trxs[i])("s",in_trxs[i].get_signed_addresses() ) );
         }
         ilog( "." );
         
         // filter out all trx that generate coins from nothing or don't pay fees
         for( uint32_t i = 0; i < in_trxs.size(); ++i )
         {
            try 
            {
                trx_stat s;
                s.eval = evaluate_signed_transaction( in_trxs[i] );
                ilog( "eval: ${eval}", ("eval",s.eval) );

               // TODO: enforce fees
                if( s.eval.fees.get_rounded_amount() < (get_fee_rate() * in_trxs[i].size()).get_rounded_amount() )
                {
                  wlog( "ignoring transaction ${trx} because it doesn't pay minimum fee ${f}\n\n state: ${s}", 
                        ("trx",in_trxs[i])("s",s.eval)("f", get_fee_rate()*in_trxs[i].size()) );
                  continue;
                }
                s.trx_idx = i + trxs.size(); // market trx will go first...
                stats.push_back( s );
            } 
            catch ( const fc::exception& e )
            {
               wlog( "unable to use trx ${t}\n ${e}", ("t", in_trxs[i] )("e",e.to_detail_string()) );
            }
         }
         ilog( "." );

         // order the trx by fees (don't sort the market orders which are added next)
         std::sort( stats.begin(), stats.end() ); 
         for( uint32_t i = 0; i < stats.size(); ++i )
         {
           ilog( "sort ${i} => ${n}", ("i", i)("n",stats[i]) );
         }
         ilog( "." );

         // consume the outputs from the market order first
         std::unordered_set<output_reference> consumed_outputs;
         for( auto itr = trxs.begin(); itr != trxs.end(); ++itr )
         {
            for( uint32_t in = 0; in < itr->inputs.size(); ++in )
            {
               FC_ASSERT( consumed_outputs.insert( itr->inputs[in].output_ref).second, 
                          "output can only be referenced once", ("in",in)("output_ref",itr->inputs[in].output_ref) )
            }
         }
         trxs.insert( trxs.end(), in_trxs.begin(), in_trxs.end() );
         ilog( "trxs: ${t}", ("t",trxs) );

         // calculate the block size as we go
         fc::datastream<size_t>  block_size;
         uint32_t conflicts = 0;

         asset    total_fees;
         uint64_t total_cdd = 0;
         uint64_t invalid_cdd = 0;
         uint64_t total_spent  = 0;

         ilog( "." );
         // insert other transactions
         for( size_t i = 0; i < stats.size(); ++i )
         {
            const signed_transaction& trx = trxs[stats[i].trx_idx]; 
            for( size_t in = 0; in < trx.inputs.size(); ++in )
            {
               ilog( "input ${in}", ("in", trx.inputs[in]) );

               if( !consumed_outputs.insert( trx.inputs[in].output_ref ).second )
               {
                    stats[i].trx_idx = uint16_t(-1); // mark it to be skipped, input conflict
                    wlog( "INPUT CONFLICT!" );
                    ++conflicts;
                    break; //in = trx.inputs.size(); // exit inner loop
               }
            }
            if( stats[i].trx_idx != uint16_t(-1) )
            {
               fc::raw::pack( block_size, trx );
               if( block_size.tellp() > MAX_BLOCK_TRXS_SIZE )
               {
                  stats.resize(i); // this trx put us over the top, we can stop processing
                                   // the other trxs.
                  break;
               }
               FC_ASSERT( i < stats.size() );
               ilog( "total fees ${tf} += ${fees},  total cdd ${tcdd} += ${cdd}", 
                     ("tf", total_fees)
                     ("fees",stats[i].eval.fees)
                     ("tcdd",total_cdd)
                     ("cdd",stats[i].eval.coindays_destroyed) );
               total_fees   += stats[i].eval.fees;
               total_cdd    += stats[i].eval.coindays_destroyed;
               invalid_cdd  += stats[i].eval.invalid_coindays_destroyed;
               total_spent  += stats[i].eval.total_spent;
            }
         }
         ilog( "." );

         // at this point we have a list of trxs to include in the block that is sorted by
         // fee and has a set of unique inputs that have all been validated against the
         // current state of the chain_database, calculate the total fees paid which are
         // destroyed as the means of paying dividends.
         
       //  wlog( "mining reward: ${mr}", ("mr", calculate_mining_reward( head_block_num() + 1) ) );
        // asset miner_fees( (total_fees.amount).high_bits(), asset::bts );
        // wlog( "miner fees: ${t}", ("t", miner_fees) );

         trx_block new_blk;
         new_blk.trxs.reserve( 1 + stats.size() - conflicts + num_orders ); 

         // add all orders first
         new_blk.trxs.insert( new_blk.trxs.begin(), trxs.begin(), trxs.begin() + num_orders );

         // add all other transactions to the block
         for( size_t i = 0; i < stats.size(); ++i )
         {
           if( stats[i].trx_idx != uint16_t(-1) )
           {
             new_blk.trxs.push_back( trxs[ stats[i].trx_idx] );
           }
         }

         new_blk.timestamp                 = fc::time_point::now();
         FC_ASSERT( new_blk.timestamp > my->head_block.timestamp );

         new_blk.block_num                 = head_block_num() + 1;
         new_blk.prev                      = my->head_block_id;
         new_blk.total_shares              = my->head_block.total_shares - total_fees.amount.high_bits(); 

         new_blk.next_difficulty           = my->head_block.next_difficulty;
         if( my->head_block.block_num > 144 )
         {
             auto     oldblock = fetch_block( my->head_block.block_num - 144 ); 
             auto     delta_time = my->head_block.timestamp - oldblock.timestamp;
             uint64_t avg_sec_per_block = delta_time.count() / 144000000;

             auto cur_tar = my->head_block.next_difficulty;
             new_blk.next_difficulty = (cur_tar * 300 /* 300 sec per block */) / avg_sec_per_block;
         }
         new_blk.total_cdd                 = total_cdd; 

         new_blk.avail_coindays            = my->head_block.avail_coindays 
                                             - total_cdd 
                                             + my->head_block.total_shares - total_spent
                                             - invalid_cdd;

         new_blk.trx_mroot = new_blk.calculate_merkle_root();

         return new_blk;
         #endif

      } FC_RETHROW_EXCEPTIONS( warn, "error generating new block" );
    }

    uint64_t chain_database::get_stake()
    {
       return my->head_block_id._hash[0];
    }
    uint64_t chain_database::get_stake2()
    {
       if( head_block_num() <= 1 ) return 0;
       if( head_block_num() == uint32_t(-1) ) return 0;
       return fetch_block( head_block_num() - 1 ).id()._hash[0];
    }
    uint64_t chain_database::current_difficulty()const
    {
       return my->head_block.next_difficulty;
    }
    uint64_t chain_database::total_shares()const
    {
       return my->head_block.total_shares;
    }
    uint64_t chain_database::available_coindays()const
    {
       return my->head_block.avail_coindays;
    }

    const block_header& chain_database::get_head_block()const { return my->head_block; }

    asset chain_database::get_fee_rate()const
    {
       return asset( uint64_t(my->head_block.next_fee) );
    }

    void chain_database::set_pow_validator( const pow_validator_ptr& v )
    {
       my->_pow_validator = v;
    }

    void chain_database::set_transaction_validator( const transaction_validator_ptr& v )
    {
       my->_trx_validator = v;
    }

}  } // bts::blockchain


