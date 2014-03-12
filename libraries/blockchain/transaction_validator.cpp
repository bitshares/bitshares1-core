#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace blockchain {
   transaction_summary::transaction_summary()
   :valid_votes(0),invalid_votes(0),fees(0)
   {}

   transaction_summary& transaction_summary::operator+=( const transaction_summary& a )
   {
      valid_votes   += a.valid_votes;
      invalid_votes += a.invalid_votes;
      fees          += a.fees;
      return *this;
   }
   transaction_evaluation_state::transaction_evaluation_state( const signed_transaction& t )
   :trx(t),valid_votes(0),invalid_votes(0)
   {
        sigs     = trx.get_signed_addresses();
        pts_sigs = trx.get_signed_pts_addresses();
   }

   bool transaction_evaluation_state::has_signature( const address& a )const
   {
        return sigs.find( a ) != sigs.end();
   }

   bool transaction_evaluation_state::has_signature( const pts_address& a )const
   {
        return pts_sigs.find( a ) != pts_sigs.end();
   }

   uint64_t transaction_evaluation_state::get_total_in( asset::type t )const
   {
       auto itr = total.find( t );
       if( itr == total.end() ) return 0;
       return itr->second.in;
   }

   uint64_t transaction_evaluation_state::get_total_out( asset::type t )const
   {
       auto itr = total.find( t );
       if( itr == total.end() ) return 0;
       return itr->second.out;
   }

   void transaction_evaluation_state::add_input_asset( asset a )
   {
       auto itr = total.find( a.unit );
       if( itr == total.end() ) total[a.unit].in = a.get_rounded_amount();
       itr->second.in += a.get_rounded_amount();
   }

   void transaction_evaluation_state::add_output_asset( asset a )
   {
       auto itr = total.find( a.unit );
       if( itr == total.end() ) total[a.unit].out = a.get_rounded_amount();
       itr->second.out += a.get_rounded_amount();
   }

   bool transaction_evaluation_state::is_output_used( uint8_t out )const
   {
       return used_outputs.find(out) != used_outputs.end();
   }

   void transaction_evaluation_state::mark_output_as_used( uint8_t out )
   {
       used_outputs.insert(out);
   }

   transaction_evaluation_state::~transaction_evaluation_state(){}

   transaction_validator::transaction_validator( chain_database* db )
   :_db(db){}

   transaction_validator::~transaction_validator(){}

   transaction_summary transaction_validator::evaluate( const signed_transaction& trx )
   {
       transaction_evaluation_state state(trx);
       return on_evaluate( state );
   }

   transaction_summary transaction_validator::on_evaluate( transaction_evaluation_state& state )
   {
       transaction_summary sum;

       state.inputs = _db->fetch_inputs( state.trx.inputs );

       /** make sure inputs are unique */
       std::unordered_set<output_reference> unique_inputs;
       for( auto in : state.trx.inputs )
       {
          FC_ASSERT( unique_inputs.insert( in.output_ref ).second, 
              "transaction references same output more than once.", ("trx",state.trx) )
       }

       /** validate all inputs */
       for( auto in : state.inputs ) 
       {
          FC_ASSERT( !in.meta_output.is_spent(), "", ("trx",state.trx) );
          validate_input( in, state );
       }


       /** validate all inputs */
       for( auto out : state.trx.outputs ) 
          validate_output( out, state );

       // TODO: move fee summary to sum and return it
       return sum;
   }

   void transaction_validator::validate_input( const meta_trx_input& in, 
                                               transaction_evaluation_state& state )
   {
       switch( in.output.claim_func )
       {
          case claim_by_pts:
             validate_pts_signature_input( in, state );            
             break;
          case claim_by_signature:
             validate_signature_input( in, state );            
             break;
          default:
             FC_ASSERT( !"Unsupported claim type", "type: ${type}", ("type",in.output.claim_func) );
       }
   }

   void transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state )
   {
       switch( out.claim_func )
       {
          case claim_by_pts:
             validate_pts_signature_output( out, state );            
             break;
          case claim_by_signature:
             validate_signature_output( out, state );            
             break;
          default:
             FC_ASSERT( !"Unsupported claim type", "type: ${type}", ("type",out.claim_func) );
       }
   }

   void transaction_validator::accumulate_coindays( uint64_t amnt, uint32_t source_block_num, transaction_evaluation_state& state )
   {
       uint32_t headnum = _db->head_block_num();
       uint32_t votes =  amnt * (headnum-source_block_num);

       if( _db->get_stake() == state.trx.stake || 
           _db->get_stake2() == state.trx.stake )
       {
          state.valid_votes += votes;
       }
       else
       {
          state.invalid_votes += votes;
       }
   }

   void transaction_validator::validate_pts_signature_input( const meta_trx_input& in, 
                                                             transaction_evaluation_state& state )
   {
       auto claim = in.output.as<claim_by_pts_output>(); 
       FC_ASSERT( state.has_signature( claim.owner ), "", ("owner",claim.owner) );
       state.add_input_asset( in.output.amount );

       if( in.output.amount.unit == 0 )
          accumulate_coindays( in.output.amount.get_rounded_amount(), in.source.block_num, state );
   }

   void transaction_validator::validate_signature_input( const meta_trx_input& in, 
                                                         transaction_evaluation_state& state )
   {
       auto claim = in.output.as<claim_by_signature_output>(); 
       FC_ASSERT( state.has_signature( claim.owner ), "", ("owner",claim.owner) );
       state.add_input_asset( in.output.amount );

       if( in.output.amount.unit == 0 )
          accumulate_coindays( in.output.amount.get_rounded_amount(), in.source.block_num, state );
   }


   void transaction_validator::validate_signature_output( const trx_output& out, 
                                   transaction_evaluation_state& state )
   {
       state.add_output_asset( out.amount );
   }

   void transaction_validator::validate_pts_signature_output( const trx_output& out, 
                                   transaction_evaluation_state& state )
   {
       state.add_output_asset( out.amount );
   }


} } // namespace bts::blockchain
