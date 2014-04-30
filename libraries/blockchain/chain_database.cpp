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
#include <iostream>
#include <iomanip>

namespace fc {
  template<> struct get_typename<std::vector<uint160>>        { static const char* name()  { return "std::vector<uint160>";  } };
  template<> struct get_typename<fc::ecc::compact_signature>  { static const char* name()  { return "fc::ecc::compact_signature";  } };
} // namespace fc



namespace bts { namespace blockchain {
    namespace ldb = leveldb;

    uint64_t to_bips( uint64_t shares, uint64_t total_shares )
    { try {
        fc::uint128   big_shares(shares);

        big_shares *= fc::uint128( int64_t(BTS_BLOCKCHAIN_BIP) );
        big_shares /= fc::uint128( total_shares );
        FC_ASSERT( big_shares.high_bits() == 0, "bips integer overflow" );
        return big_shares.low_bits(); //(BTS_BLOCKCHAIN_BIP * shares) / total_shares;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("shares",shares)("total_shares",total_shares) ) }

    namespace detail
    {
       struct vote_del
       {
          vote_del( int64_t v = 0, uint32_t del = 0 )
          :votes(v),delegate_id(del){}
          int64_t votes;
          uint32_t delegate_id;
          friend bool operator == ( const vote_del& a, const vote_del& b )
          {
             return a.votes == b.votes && a.delegate_id == b.delegate_id;
          }
          friend bool operator < ( const vote_del& a, const vote_del& b )
          {
             return a.votes > b.votes ? true : (a.votes == b.votes ? a.delegate_id > b.delegate_id : false);
          }
       };

      // TODO: .01 BTC update private members to use _member naming convention
      class chain_database_impl
      {
         public:
            chain_database_impl()
            { }
            chain_database*                                     _self;

            //std::unique_ptr<ldb::DB> blk_id2num;  // maps blocks to unique IDs
            bts::db::level_map<block_id_type,uint32_t>          blk_id2num;
            bts::db::level_map<uint160,trx_num>                 trx_id2num;
            bts::db::level_map<trx_num,meta_trx>                meta_trxs;
            bts::db::level_map<uint32_t,signed_block_header>    blocks;
            bts::db::level_map<uint32_t,std::vector<uint160> >  block_trxs;

            bts::db::level_map< uint32_t, name_record >         _delegate_records;
            bts::db::level_map< std::string, name_record >      _name_records;

            /**
             *  track the delegate votes by rank
             */
            std::set<vote_del>                                  _votes_to_delegate;
            std::map<uint32_t, int64_t>                         _delegate_to_votes;

            pow_validator_ptr                                   _pow_validator;
            transaction_validator_ptr                           _trx_validator;
            address                                             _trustee;


            /** cache this information because it is required in many calculations  */
            trx_block                                           head_block;
            block_id_type                                       head_block_id;

            void update_delegate( const name_record& rec  )
            {
                auto new_votes = rec.votes_for - rec.votes_against;
                auto itr = _delegate_to_votes.find( rec.delegate_id );
                if( itr != _delegate_to_votes.end() )
                {
                    ilog( "old votes: ${v}  new votes ${nv}", ("v",itr->second)("nv",new_votes) );
                    _votes_to_delegate.erase( vote_del( itr->second, itr->first ));
                }
                _votes_to_delegate.insert( vote_del(new_votes, rec.delegate_id) );
                _delegate_to_votes[ rec.delegate_id ] = new_votes;

                wlog( "store delegate ${d}", ("d",rec) );
                _delegate_records.store( rec.delegate_id, rec );
            }

            void mark_spent( const output_reference& o, const trx_num& intrx, uint16_t in )
            {
               auto tid    = trx_id2num.fetch( o.trx_hash );
               meta_trx   mtrx   = meta_trxs.fetch( tid );
               FC_ASSERT( mtrx.meta_outputs.size() > o.output_idx.value );

               mtrx.meta_outputs[o.output_idx.value].trx_id    = intrx;
               mtrx.meta_outputs[o.output_idx.value].input_num = in;

               meta_trxs.store( tid, mtrx );
            }

            trx_output get_output( const output_reference& ref )
            { try {
               auto tid    = trx_id2num.fetch( ref.trx_hash );
               meta_trx   mtrx   = meta_trxs.fetch( tid );
               FC_ASSERT( mtrx.outputs.size() > ref.output_idx.value );
               return mtrx.outputs[ref.output_idx.value];
            } FC_RETHROW_EXCEPTIONS( warn, "", ("ref",ref) ) }

            /**
             *   Stores a transaction and updates the spent status of all
             *   outputs doing one last check to make sure they are unspent.
             */
            void store( const signed_transaction& t, const trx_num& tn )
            {
               //ilog( "trxid: ${id}   ${tn}\n\n  ${trx}\n\n", ("id",t.id())("tn",tn)("trx",t) );

               trx_id2num.store( t.id(), tn );
               meta_trxs.store( tn, meta_trx(t) );

               for( uint16_t i = 0; i < t.inputs.size(); ++i )
               {
                  mark_spent( t.inputs[i].output_ref, tn, i );
               }
            }

            /** the genesis block requires some special treatment and initialization */
            void store_genesis( const trx_block& b, 
                                const signed_transactions& deterministic_trxs, 
                                const block_evaluation_state_ptr& state  )
            { try {
                std::vector<uint160> trxs_ids;
                std::map<int32_t,uint64_t> delegate_votes;
                for( uint32_t i = 1; i <= 100; ++i )
                {
                   delegate_votes[i] = 0;
                }
                for( uint32_t cur_trx = 0 ; cur_trx < b.trxs.size(); ++cur_trx )
                {
                    store( b.trxs[cur_trx], trx_num( b.block_num, cur_trx) );
                    trxs_ids.push_back( b.trxs[cur_trx].id() );
                    if( cur_trx == 0 )
                    {
                       for( uint32_t o = 0; o < b.trxs[cur_trx].outputs.size(); ++o )
                       {
                          if( b.trxs[cur_trx].outputs[o].claim_func == claim_name )
                          {
                             auto claim = b.trxs[cur_trx].outputs[o].as<claim_name_output>();
                             update_name_record( claim.name, claim );
                       //    if( claim.delegate_id != 0 )
                             update_delegate( name_record( claim ) );
                          }
                       }
                    }
                    else // cur_trx != 0
                    {
                       ilog( " processing transaction ${o}", ("o",cur_trx) );
                       name_record rec = _delegate_records.fetch( b.trxs[cur_trx].vote );
                       // first transaction registers names... the rest are initial balance
                       for( uint32_t o = 0; o < b.trxs[cur_trx].outputs.size(); ++o )
                       {
                          ilog( "   processing output  ${o}  ${data}", ("o",o)("data",b.trxs[cur_trx].outputs[o]) );
                          FC_ASSERT( delegate_votes.find( b.trxs[cur_trx].vote ) != delegate_votes.end() );
                          delegate_votes[b.trxs[cur_trx].vote] += b.trxs[cur_trx].outputs[o].amount.get_rounded_amount();
                          ilog( "total_shares: ${total}", ("total",b.total_shares) );
                          rec.votes_for += to_bips( b.trxs[cur_trx].outputs[o].amount.get_rounded_amount(), b.total_shares );
                          ilog( "votes for: ${v}", ("v",rec.votes_for) );
                          ilog( "rec: ${rc}", ("rc",rec) );
                       }
                       ilog( "updating delegate..." );
                       update_delegate( rec );
                    }
                }
                elog( "total votes:\n ${e}", ("e",fc::json::to_pretty_string( delegate_votes) ) );
                uint64_t sum = 0;
                for( auto i : delegate_votes )
                {
                   sum += i.second;
                }
                elog( "grand total: ${g}", ("g",sum) );

                head_block    = b;
                head_block_id = b.id();

                blocks.store( b.block_num, b );
                block_trxs.store( b.block_num, trxs_ids );

                blk_id2num.store( b.id(), b.block_num );
            }  FC_RETHROW_EXCEPTIONS( warn, "error storing genesis block " ) } // store_genesis


            void store( const trx_block& b, 
                        const signed_transactions& deterministic_trxs, 
                        const block_evaluation_state_ptr& state  )
            { try {

                if( b.block_num == 0 )
                {
                   store_genesis( b, deterministic_trxs, state );
                   return;
                }
                std::vector<uint160> trxs_ids;

                // store individual transactions
                for( uint32_t cur_trx = 0; cur_trx < b.trxs.size(); ++cur_trx )
                {
                   store( b.trxs[cur_trx], trx_num( b.block_num, cur_trx) );
                   trxs_ids.push_back( b.trxs[cur_trx].id() );
                }

                // store deterministic transactions
                for( const signed_transaction& trx : deterministic_trxs )
                {
                   store( trx, trx_num( b.block_num, trxs_ids.size() ) );
                   // TODO: why don't we include this here... do determinsitic trxs not
                   // get included in the merkel root... does this hinder light weight
                   // clients.
                  // trxs_ids.push_back( trx.id() );
                }
                head_block    = b;
                head_block_id = b.id();

                blocks.store( b.block_num, b );
                block_trxs.store( b.block_num, trxs_ids );

                blk_id2num.store( b.id(), b.block_num );
                

                // update name name records...
                for( auto item : state->_name_outputs )
                {
                   update_name_record( item.first, item.second );
                }

                // update votes
                for( auto item : state->_input_votes )
                {
                   auto rec = _delegate_records.fetch( abs(item.first) );
                   if( item.first < 0 )
                      rec.votes_against -= to_bips(item.second,b.total_shares);
                   else
                      rec.votes_for     -= to_bips(item.second,b.total_shares);
                   update_delegate( rec );
                }
                for( auto item : state->_output_votes )
                {
                   auto rec = _delegate_records.fetch( abs(item.first) );
                   if( item.first < 0 )
                      rec.votes_against += to_bips( item.second, b.total_shares );
                   else
                      rec.votes_for     += to_bips( item.second, b.total_shares );
                   update_delegate( rec );
                }

            } FC_RETHROW_EXCEPTIONS( warn, "" ) }

            void update_name_record( const std::string& name, const claim_name_output& out )
            {
                auto current_record = _self->lookup_name( name );
                if( current_record )
                {
                   current_record->delegate_id = out.delegate_id;
                   current_record->data        = out.data;
                   current_record->owner       = out.owner;
                   _name_records.store( name, *current_record );
                   update_delegate( *current_record );
                }
                else
                {
                   name_record rec;
                   rec.delegate_id = out.delegate_id;
                   rec.data        = out.data;
                   rec.owner       = out.owner;
                   rec.name        = name;
                   _name_records.store( name, rec );
                   update_delegate( rec );
                }
            }
      };
    }

     chain_database::chain_database()
     :my( new detail::chain_database_impl() )
     {
         my->_self = this;
         my->_trx_validator = std::make_shared<transaction_validator>(this);
         my->_pow_validator = std::make_shared<pow_validator>(this);
     }

     chain_database::~chain_database()
     {
     }
     fc::optional<name_record> chain_database::lookup_name( const std::string& name )
     {
        auto itr = my->_name_records.find( name );
        if( itr.valid() )
           return itr.value();
        return fc::optional<name_record>();
     }

     fc::optional<name_record> chain_database::lookup_delegate( uint16_t del )
     { try {
        auto itr = my->_delegate_records.find( del );
        if( itr.valid() )
           return itr.value();
        return fc::optional<name_record>();
     } FC_RETHROW_EXCEPTIONS( warn, "delegate id: ${id}", ("id",del) ) }

     std::vector<name_record> chain_database::get_names( const std::string& first, uint32_t count )
     { try {
          std::vector<name_record> records;
          records.reserve(100);
          auto name_itr = my->_name_records.lower_bound( first ); 
          uint32_t num = 0;
          if( name_itr.valid() && num < count )
          {
             records.push_back( name_itr.value() );
             ++name_itr;
             ++num;
          }
          return records;
     } FC_RETHROW_EXCEPTIONS( warn, "", ("first",first)("count",count) ) }

     std::vector<name_record> chain_database::get_delegates( uint32_t count )
     {
        std::vector<name_record> results;
        results.reserve( 100 );

        uint32_t i = 0;
        for( auto del : my->_votes_to_delegate )
        {
           results.push_back( *lookup_delegate( del.delegate_id ) );   
           ++i;
           if( i == count ) return results;
        }
        return results;
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
         my->_delegate_records.open( dir / "delegate_records", create );
         my->_name_records.open( dir / "name_records", create );


         // read the last block from the DB
         my->blocks.last( my->head_block.block_num, my->head_block );
         if( my->head_block.block_num != uint32_t(-1) )
         {
            my->head_block_id = my->head_block.id();


            auto itr = my->_delegate_records.begin();
            while( itr.valid() )
            {
               auto rec = itr.value();
               auto votes = rec.votes_for - rec.votes_against;
               my->_votes_to_delegate.insert( detail::vote_del( votes, rec.delegate_id ) );
               my->_delegate_to_votes[rec.delegate_id] = votes;
               ++itr;
            }
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

    signed_block_header chain_database::fetch_block( uint32_t block_num )
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

    std::vector<meta_trx_input> chain_database::fetch_inputs( 
                             const std::vector<trx_input>& inputs, uint32_t head )
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

             if( inputs[i].output_ref.output_idx.value >= trx.meta_outputs.size() )
             {
                FC_THROW_EXCEPTION( exception, "Input ${i} references invalid output from transaction ${trx}",
                                    ("i",inputs[i])("trx", trx) );
             }
             if( inputs[i].output_ref.output_idx.value >= trx.outputs.size() )
             {
                FC_THROW_EXCEPTION( exception, "Input ${i} references invalid output from transaction ${t}",
                                    ("i",inputs[i])("o", trx) );
             }

             meta_trx_input metin;
             metin.source       = tn;
             metin.delegate_id  = trx.vote;
             metin.output_num   = inputs[i].output_ref.output_idx;
             metin.output       = trx.outputs[metin.output_num];
             metin.meta_output  = trx.meta_outputs[metin.output_num];
             metin.data         = inputs[i].input_data;
             rtn.push_back( metin );

            } FC_RETHROW_EXCEPTIONS( warn, "error fetching input [${i}] ${in}", ("i",i)("in", inputs[i]) );
          }
          return rtn;
       } FC_RETHROW_EXCEPTIONS( warn, "error fetching transaction inputs", ("inputs", inputs) );
    }


    void validate_unique_inputs( const signed_transactions& deterministic_trxs, const signed_transactions& trxs )
    {
       std::unordered_set<output_reference> ref_outs;
       for( const signed_transaction& trx : deterministic_trxs )
       {
            for( const trx_input& in : trx.inputs )
            {
               FC_ASSERT( ref_outs.insert( in.output_ref ).second, "duplicate input detected" );
            }
       }
       for( const signed_transaction& trx : trxs )
       {
            for( const trx_input& in : trx.inputs )
            {
               FC_ASSERT( ref_outs.insert( in.output_ref ).second, "duplicate input detected" );
            }
       }
    }

    signed_transactions chain_database::generate_deterministic_transactions()
    {
       signed_transactions trxs;
       // TODO: move all unspent outputs over 1 year old to new outputs and charge a 5% fee

       return trxs;
    }

    block_evaluation_state_ptr chain_database::validate( const trx_block& b, const signed_transactions& deterministic_trxs )
    { try {
        auto block_state = my->_trx_validator->create_block_state();
        if( b.block_num == 0 ) { return block_state; } // don't check anything for the genesis block;
        // TODO: replace hardcoded _trustee with a lookup of the trustee for the timestamp on this block
        FC_ASSERT( b.signee() == my->_trustee );
        FC_ASSERT( b.version      == 0                                                         );
        FC_ASSERT( b.block_num    == head_block_num() + 1                                      );
        FC_ASSERT( b.prev         == my->head_block_id                                         );
        /// time stamps from the future are not allowed
        FC_ASSERT( b.next_fee     == b.calculate_next_fee( my->head_block.next_fee,  b.block_size() ), "",
                   ("b.next_fee",b.next_fee)("b.calculate_next_fee", b.calculate_next_fee( my->head_block.next_fee, b.block_size()))
                   ("get_fee_rate",get_fee_rate())("b.size",b.block_size())
                   );

        FC_ASSERT( b.timestamp.sec_since_epoch() % BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC == 0 );

        FC_ASSERT( b.timestamp    <= (my->_pow_validator->get_time() + fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC/4)), "",
                   ("b.timestamp", b.timestamp)("future",my->_pow_validator->get_time()+ fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC/4)));


        FC_ASSERT( b.timestamp    > fc::time_point(my->head_block.timestamp)  );


        validate_unique_inputs( b.trxs, deterministic_trxs );

        FC_ASSERT( b.trx_mroot == b.calculate_merkle_root(deterministic_trxs) );

        transaction_summary summary;
        transaction_summary trx_summary;
        int32_t last = b.trxs.size()-1;
        uint64_t fee_rate = get_fee_rate();
        for( int32_t i = 0; i <= last; ++i )
        {
            trx_summary = my->_trx_validator->evaluate( b.trxs[i], block_state );
            FC_ASSERT( b.trxs[i].version == 0 );
            FC_ASSERT( trx_summary.fees >= (b.trxs[i].size() * fee_rate)/1000 );
            summary += trx_summary;
        }

        for( auto strx : deterministic_trxs )
        {
            summary += my->_trx_validator->evaluate( strx, block_state );
        }

        FC_ASSERT( b.next_reward     == b.calculate_next_reward( b.next_reward, summary.fees ) )
        FC_ASSERT( b.total_shares    == my->head_block.total_shares - summary.fees, "",
                   ("b.total_shares",b.total_shares)
                   ("head_block.total_shares",my->head_block.total_shares)
                   ("summary.fees",summary.fees) );

        return block_state;

    } FC_RETHROW_EXCEPTIONS( warn, "error validating block" ) }

    /**
     *  Attempts to append block b to the block chain with the given trxs.
     */
    void chain_database::push_block( const trx_block& b )
    { try {
        auto deterministic_trxs = generate_deterministic_transactions();
        auto state = validate( b, deterministic_trxs );
        store( b, deterministic_trxs, state );
      } FC_RETHROW_EXCEPTIONS( warn, "unable to push block", ("b", b) );
    } // chain_database::push_block

    void chain_database::store( const trx_block& blk, const signed_transactions& deterministic_trxs, const block_evaluation_state_ptr& state )
    {
        my->store( blk, deterministic_trxs, state );
    }

    /**
     *  Removes the top block from the stack and marks all spent outputs as
     *  unspent.
     */
    trx_block chain_database::pop_block()
    {
       FC_ASSERT( !"TODO: implement pop_block" );
    }


    uint64_t chain_database::get_stake()
    {
       return my->head_block_id._hash[0];
    }

    uint64_t chain_database::total_shares()const
    {
       return my->head_block.total_shares;
    }

    pow_validator_ptr         chain_database::get_pow_validator()const { return my->_pow_validator; }
    transaction_validator_ptr chain_database::get_transaction_validator()const { return my->_trx_validator; }

    const signed_block_header& chain_database::get_head_block()const { return my->head_block; }

    uint64_t chain_database::get_fee_rate()const
    {
       return my->head_block.next_fee;
    }

    void chain_database::set_pow_validator( const pow_validator_ptr& v )
    {
       my->_pow_validator = v;
    }

    void chain_database::set_transaction_validator( const transaction_validator_ptr& v )
    {
       my->_trx_validator = v;
    }

    void chain_database::set_trustee( const address& a )
    {
       my->_trustee = a;
    }

    address chain_database::get_trustee()const
    {
       return my->_trustee;
    }

    void chain_database::evaluate_transaction( const signed_transaction& trx )
    {
       get_transaction_validator()->evaluate( trx, get_transaction_validator()->create_block_state() );
    }
    uint32_t  chain_database::get_new_delegate_id()const
    {
       uint32_t new_id = rand();
       while( my->_delegate_to_votes.find(new_id) != my->_delegate_to_votes.end() )
            new_id = fc::time_point::now().time_since_epoch().count() ^ rand();
       return new_id;
    }

}  } // bts::blockchain


