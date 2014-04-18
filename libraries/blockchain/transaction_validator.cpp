#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>

#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
   transaction_summary::transaction_summary()
   :valid_votes(0),invalid_votes(0),spent(0),fees(0)
   {}

   transaction_summary& transaction_summary::operator+=( const transaction_summary& a )
   {
      valid_votes   += a.valid_votes;
      invalid_votes += a.invalid_votes;
      spent         += a.spent;
      fees          += a.fees;
      return *this;
   }
   transaction_evaluation_state::transaction_evaluation_state( const signed_transaction& t )
   :trx(t),valid_votes(0),invalid_votes(0),spent(0)
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

   int64_t transaction_evaluation_state::get_total_in( asset_type t )const
   {
       auto itr = total.find( t );
       if( itr == total.end() ) return 0;
       return itr->second.in;
   }

   int64_t transaction_evaluation_state::get_total_out( asset_type t )const
   {
       auto itr = total.find( t );
       if( itr == total.end() ) return 0;
       return itr->second.out;
   }

   int64_t transaction_evaluation_state::get_required_fees( asset_type t )const
   {
       auto itr = total.find( t );
       if( itr == total.end() ) return 0;
       return itr->second.required_fees;
   }
   void transaction_evaluation_state::add_name_input( const claim_name_output& o )
   {
       FC_ASSERT( name_inputs.find( o.name ) == name_inputs.end() );
       name_inputs[o.name] = o;
   }
   void block_evaluation_state::add_input_delegate_votes( int32_t did, const asset& votes )
   {
      auto itr = _input_votes.find(did);
      if( itr == _input_votes.end() )
         _input_votes[did] = votes.get_rounded_amount();
      else
         itr->second += votes.get_rounded_amount();
   }
   void block_evaluation_state::add_output_delegate_votes( int32_t did, const asset& votes )
   {
      auto itr = _output_votes.find(did);
      if( itr == _output_votes.end() )
         _output_votes[did] = votes.get_rounded_amount();
      else
         itr->second += votes.get_rounded_amount();
   }

   void transaction_evaluation_state::add_input_asset( asset a )
   {
       auto itr = total.find( a.unit );
       if( itr == total.end() ) total[a.unit].in = a.get_rounded_amount();
       else itr->second.in += a.get_rounded_amount();
   }

   void transaction_evaluation_state::add_output_asset( asset a )
   {
       auto itr = total.find( a.unit );
       if( itr == total.end() ) total[a.unit].out = a.get_rounded_amount();
       else itr->second.out += a.get_rounded_amount();
   }
   void transaction_evaluation_state::add_required_fees( asset a )
   {
       auto itr = total.find( a.unit );
       if( itr == total.end() ) total[a.unit].required_fees = a.get_rounded_amount();
       else itr->second.required_fees += a.get_rounded_amount();
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


   block_evaluation_state_ptr transaction_validator::create_block_state()const
   {
      return std::make_shared<block_evaluation_state>();
   }

   transaction_summary transaction_validator::evaluate( const signed_transaction& trx, 
                                                        const block_evaluation_state_ptr& block_state )
   {
       transaction_evaluation_state state(trx);
       return on_evaluate( state, block_state );
   }

   transaction_summary transaction_validator::on_evaluate( transaction_evaluation_state& state, 
                                                           const block_evaluation_state_ptr& block_state )
   { try {
       transaction_summary sum;

       state.inputs = _db->fetch_inputs( state.trx.inputs );
       auto trx_delegate = _db->lookup_delegate( state.trx.vote );
       FC_ASSERT( !!trx_delegate, "unable to find delegate id ${id}", ("id",state.trx.vote) );

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
          validate_input( in, state, block_state );
       }


       /** validate all inputs */
       for( auto out : state.trx.outputs ) 
          validate_output( out, state, block_state );

       state.balance_assets();

       sum.valid_votes      = state.valid_votes;
       sum.invalid_votes    = state.invalid_votes;
       sum.spent            = state.spent;
       sum.fees             = state.get_total_in(0) - state.get_total_out(0);
       if( state.get_required_fees() >= 0 )
       {
          FC_ASSERT( sum.fees >= state.get_required_fees(0), "",
                     ("fees",sum.fees)("required",state.get_required_fees()));
       }
       return sum;
   } FC_RETHROW_EXCEPTIONS( warn, "") }

   void transaction_evaluation_state::balance_assets()const
   {
      for( auto itr = total.begin(); itr != total.end(); ++itr )
      {
         if( itr->first != 0 )
         {
            FC_ASSERT( itr->second.out == itr->second.in );
         }
      }
   }

   void transaction_validator::validate_input( const meta_trx_input& in, 
                                               transaction_evaluation_state& state,
                                               const block_evaluation_state_ptr& block_state
                                               )
   { try {
       switch( in.output.claim_func )
       {
          case claim_by_pts:
             validate_pts_signature_input( in, state, block_state );            
             break;
          case claim_by_signature:
             validate_signature_input( in, state, block_state );            
             break;
          case claim_name:
             validate_name_input( in, state, block_state );            
             break;
          default:
             FC_ASSERT( !"Unsupported claim type", "type: ${type}", ("type",in.output.claim_func) );
       }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("in",in)("state",state) ) }

   void transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state,
                                               const block_evaluation_state_ptr& block_state )
   {
       switch( out.claim_func )
       {
          case claim_by_pts:
             validate_pts_signature_output( out, state, block_state );            
             break;
          case claim_by_signature:
             validate_signature_output( out, state, block_state );            
             break;
          case claim_name:
             validate_name_output( out, state, block_state );            
             break;
          default:
             FC_ASSERT( !"Unsupported claim type", "type: ${type}", ("type",out.claim_func) );
       }
   }

   void transaction_validator::accumulate_votes( uint64_t amnt, uint32_t source_block_num, 
                                                 transaction_evaluation_state& state )
   {
       uint32_t headnum = _db->head_block_num();
       uint32_t votes =  amnt * (headnum-source_block_num+1);

       if( _db->get_stake() == state.trx.stake )
       {
          state.valid_votes += votes;
       }
       else
       {
          state.invalid_votes += votes;
       }
       state.spent += amnt;
   }

   void transaction_validator::validate_pts_signature_input( const meta_trx_input& in, 
                                                             transaction_evaluation_state& state,
                                                             const block_evaluation_state_ptr& block_state )
   {
       auto claim = in.output.as<claim_by_pts_output>(); 
       FC_ASSERT( state.has_signature( claim.owner ), "", ("owner",claim.owner) );
       state.add_input_asset( in.output.amount );

       if( in.output.amount.unit == 0 )
       {
          accumulate_votes( in.output.amount.get_rounded_amount(), in.source.block_num, state );
          block_state->add_input_delegate_votes( in.delegate_id, in.output.amount );
          block_state->add_output_delegate_votes( state.trx.vote, in.output.amount );
       }
   }

   void transaction_validator::validate_signature_input( const meta_trx_input& in, 
                                                         transaction_evaluation_state& state,
                                                         const block_evaluation_state_ptr& block_state )
   {
       auto claim = in.output.as<claim_by_signature_output>(); 
       FC_ASSERT( state.has_signature( claim.owner ), "", ("owner",claim.owner)("sigs",state.sigs) );
       state.add_input_asset( in.output.amount );

       if( in.output.amount.unit == 0 )
       {
          accumulate_votes( in.output.amount.get_rounded_amount(), in.source.block_num, state );
          block_state->add_input_delegate_votes( in.delegate_id, in.output.amount );
          block_state->add_output_delegate_votes( state.trx.vote, in.output.amount );
       }
   }

   void transaction_validator::validate_name_input( const meta_trx_input& in, 
                                                         transaction_evaluation_state& state,
                                                         const block_evaluation_state_ptr& block_state )
   {
       auto claim = in.output.as<claim_name_output>(); 
       FC_ASSERT( state.has_signature( address(claim.owner) ), "", ("owner",claim.owner)("sigs",state.sigs) );
       state.add_name_input( claim );
       state.add_input_asset( in.output.amount );

       if( in.output.amount.unit == 0 )
       {
          accumulate_votes( in.output.amount.get_rounded_amount(), in.source.block_num, state );
          block_state->add_input_delegate_votes( in.delegate_id, in.output.amount );
          block_state->add_output_delegate_votes( state.trx.vote, in.output.amount );
       }
   }


   void transaction_validator::validate_signature_output( const trx_output& out, 
                                                          transaction_evaluation_state& state,
                                                          const block_evaluation_state_ptr& block_state )
   {
       state.add_output_asset( out.amount );
   }

   void transaction_validator::validate_pts_signature_output( const trx_output& out, 
                                                              transaction_evaluation_state& state,
                                                              const block_evaluation_state_ptr& block_state )
   {
       state.add_output_asset( out.amount );
   }

   void transaction_validator::validate_name_output( const trx_output& out, 
                                                     transaction_evaluation_state& state,
                                                     const block_evaluation_state_ptr& block_state )
   {
       auto claim = out.as<claim_name_output>(); 
       block_state->add_name_output( claim );
       if( !state.has_name_input( claim ) )
       {
          auto name_rec = _db->lookup_name( claim.name );
          FC_ASSERT( !name_rec );
       }
       FC_ASSERT( out.amount.unit == 0 );
       state.add_output_asset( out.amount );
   }



} } // namespace bts::blockchain
