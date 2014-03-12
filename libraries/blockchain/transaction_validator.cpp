#include <bts/blockchain/transaction_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {

   uint64_t transaction_evaluation_state::get_total_in( asset::type t )const
   {
       auto itr = total_in.find( t );
       if( itr == total_in.end() ) return 0;
       return itr->second;
   }

   uint64_t transaction_evaluation_state::get_total_out( asset::type t )const
   {
       auto itr = total_out.find( t );
       if( itr == total_out.end() ) return 0;
       return itr->second;
   }

   void transaction_evaluation_state::add_input_asset( asset a )
   {
       auto itr = total_in.find( a.unit );
       if( itr == total_in.end() ) total_in[a.unit] = a.get_rounded_amount();
       itr->second += a.get_rounded_amount();
   }

   void transaction_evaluation_state::add_output_asset( asset a )
   {
       auto itr = total_out.find( a.unit );
       if( itr == total_out.end() ) total_out[a.unit] = a.get_rounded_amount();
       itr->second += a.get_rounded_amount();
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

   void transaction_validator::validate_pts_signature_input( const meta_trx_input& in, 
                                                             transaction_evaluation_state& state )
   {

   }

   void transaction_validator::validate_signature_input( const meta_trx_input& in, 
                                                         transaction_evaluation_state& state )
   {

   }


   void validate_signature_output( const trx_output& out, 
                                   transaction_evaluation_state& state )
   {

   }

   void validate_pts_signature_output( const trx_output& out, 
                                       transaction_evaluation_state& state )
   {
        
   }


} } // namespace bts::blockchain
