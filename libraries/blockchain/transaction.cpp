#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/fire_operation.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/error_codes.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/log/logger.hpp>
#include <fc/io/raw_variant.hpp>
#include <iostream>
#include <fc/io/json.hpp>

namespace bts { namespace blockchain {

   static const fc::microseconds one_year = fc::seconds( 60*60*24*365 );

   digest_type transaction::digest( const digest_type& chain_id )const
   {
      fc::sha256::encoder enc;
      fc::raw::pack(enc,*this);
      fc::raw::pack(enc,chain_id);
      return enc.result();
   }
   size_t signed_transaction::data_size()const
   {
      fc::datastream<size_t> ds;
      fc::raw::pack(ds,*this);
      return ds.tellp();
   }
   transaction_id_type signed_transaction::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack(enc,*this);
      return fc::ripemd160::hash( enc.result() );
   }
   void signed_transaction::sign( const fc::ecc::private_key& signer, const digest_type& chain_id )
   {
      signatures.push_back( signer.sign_compact( digest(chain_id) ) );
   }

   transaction_evaluation_state::transaction_evaluation_state( const chain_interface_ptr& current_state, digest_type chain_id )
   :_current_state( current_state ),_chain_id(chain_id)
   {
   }

   transaction_evaluation_state::~transaction_evaluation_state()
   {
   }
   void transaction_evaluation_state::reset()
   {
      signed_keys.clear();
      balance.clear();
      deposits.clear();
      withdraws.clear();
      net_delegate_votes.clear();
      required_deposits.clear();
      validation_error_code = 0;
      validation_error_data = fc::variant();
   }

   void transaction_evaluation_state::evaluate( const signed_transaction& trx_arg )
   { try {
      reset();

      auto trx_id = trx_arg.id();
      otransaction_location current_loc = _current_state->get_transaction_location( trx_id );
      if( !!current_loc )
         fail( BTS_DUPLICATE_TRANSACTION, "transaction has already been processed" );

      trx = trx_arg;
      auto digest = trx_arg.digest( _chain_id );
      for( auto sig : trx.signatures )
      {
         auto key = fc::ecc::public_key( sig, digest ).serialize();
         signed_keys.insert( address(key) );
         signed_keys.insert( address(pts_address(key,false,56) ) );
         signed_keys.insert( address(pts_address(key,true,56) )  );
         signed_keys.insert( address(pts_address(key,false,0) )  );
         signed_keys.insert( address(pts_address(key,true,0) )   );
      }
      for( auto op : trx.operations )
      {
         evaluate_operation( op );
      }
      post_evaluate();
      validate_required_fee();
      update_delegate_votes();
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx_arg) ) }

   /**
    *  Process all fees and update the asset records.
    */
   void transaction_evaluation_state::post_evaluate()
   { try {

      for( auto fee : balance )
      {
         if( fee.second < 0 ) fail( BTS_INSUFFICIENT_FUNDS, fc::variant(fee) );
         auto asset_record = _current_state->get_asset_record( fee.first );
         FC_ASSERT( !!asset_record, "unable to find asset ${a}", ("a",fee.first) );
         asset_record->collected_fees += fee.second;
         asset_record->current_share_supply -= fee.second;
         _current_state->store_asset_record( *asset_record );
      }
      for( auto req_deposit : required_deposits )
      {
         auto provided_itr = provided_deposits.find( req_deposit.first );
         
         if( provided_itr->second < req_deposit.second )
            fail( BTS_MISSING_REQUIRED_DEPOSIT, fc::variant(req_deposit) );
      }

      for( auto sig : required_keys )
      {
         if( signed_keys.find( sig ) == signed_keys.end() )
            fail( BTS_MISSING_SIGNATURE, fc::variant(sig) );
      }

   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void transaction_evaluation_state::validate_required_fee()
   { try {
      share_type required_fee = (_current_state->get_fee_rate() * trx.data_size())/1000;
      auto fee_itr = balance.find( 0 );
      FC_ASSERT( fee_itr != balance.end() );
      if( fee_itr->second < required_fee ) 
         fail( BTS_INSUFFICIENT_FEE_PAID, fc::variant() );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void transaction_evaluation_state::update_delegate_votes()
   {
      auto asset_rec = _current_state->get_asset_record( BASE_ASSET_ID );
      auto max_votes = 2 * (asset_rec->current_share_supply / BTS_BLOCKCHAIN_NUM_DELEGATES);

      for( auto del_vote : net_delegate_votes )
      {
         auto del_rec = _current_state->get_account_record( del_vote.first );
         FC_ASSERT( !!del_rec );
         del_rec->adjust_votes_for( del_vote.second.votes_for );
         del_rec->adjust_votes_against( del_vote.second.votes_against );
         if( del_rec->votes_for() > max_votes || del_rec->votes_against() > max_votes )
            fail( BTS_DELEGATE_MAX_VOTE_LIMIT, fc::variant() );
         _current_state->store_account_record( *del_rec );
      }
   }

   void transaction_evaluation_state::evaluate_operation( const operation& op )
   {
      switch( (operation_type_enum)op.type  )
      {
         case null_op_type:
            FC_ASSERT( !"Invalid operation" );
            break;
         case withdraw_op_type:
            evaluate_withdraw( op.as<withdraw_operation>() );
            break;
         case deposit_op_type:
            evaluate_deposit( op.as<deposit_operation>() );
            break;
         case register_account_op_type:
            evaluate_register_account( op.as<register_account_operation>() );
            break;
         case update_account_op_type:
            evaluate_update_account( op.as<update_account_operation>() );
            break;
         case withdraw_pay_op_type:
            evaluate_withdraw_pay( op.as<withdraw_pay_operation>() );
         case create_asset_op_type:
            evaluate_create_asset( op.as<create_asset_operation>() );
            break;
         case update_asset_op_type:
            evaluate_update_asset( op.as<update_asset_operation>() );
            break;
         case issue_asset_op_type:
            evaluate_issue_asset( op.as<issue_asset_operation>() );
            break;
         case fire_delegate_op_type:
            evaluate_fire_operation( op.as<fire_delegate_operation>() );
            break;
         case submit_proposal_op_type:
            evaluate_submit_proposal( op.as<submit_proposal_operation>() );
            break;
         case vote_proposal_op_type:
            evaluate_vote_proposal( op.as<vote_proposal_operation>() );
            break;
         case bid_op_type:
            evaluate_bid( op.as<bid_operation>() );
            break;
         case ask_op_type:
            evaluate_ask( op.as<ask_operation>() );
            break;
         case short_op_type:
            evaluate_short( op.as<short_operation>() );
            break;
         case cover_op_type:
            evaluate_cover( op.as<cover_operation>() );
            break;
         default:
            FC_ASSERT( false, "Evaluation for op type ${t} not implemented!", ("t", op.type) );
            break;
      }
   }
   void transaction_evaluation_state::evaluate_submit_proposal( const submit_proposal_operation& op )
   { try {
       ///  signed by a registered delegate
       auto delegate_record = _current_state->get_account_record( op.submitting_delegate_id );
       FC_ASSERT( !!delegate_record && delegate_record->is_delegate(),
                  "A proposal may only be submitted by an active and registered delegate" );

       /// signed by a current delegate
       FC_ASSERT( _current_state->is_active_delegate(op.submitting_delegate_id), 
                  "A proposal may only be submitted by an active delegate" );

       proposal_record new_proposal;
       new_proposal.id = _current_state->new_proposal_id();
       new_proposal.submitting_delegate_id = op.submitting_delegate_id;
       new_proposal.submission_date = op.submission_date;
       new_proposal.subject = op.subject;
       new_proposal.body = op.body;
       new_proposal.proposal_type = op.proposal_type;
       new_proposal.data = op.data;

       add_required_signature( delegate_record->active_address() );
       _current_state->store_proposal_record( new_proposal );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_vote_proposal( const vote_proposal_operation& op )
   { try {
       ///  signed by a registered delegate
       auto delegate_record = _current_state->get_account_record( op.id.delegate_id );
       FC_ASSERT( !!delegate_record && delegate_record->is_delegate(),
                  "A proposal may only be voted by an active and registered delegate" );

       /// signed by a current delegate
       FC_ASSERT( _current_state->is_active_delegate(op.id.delegate_id), 
                  "A proposal may only be submitted by an active delegate" );

       add_required_signature( delegate_record->active_address() );

       proposal_vote new_vote;
       new_vote.id = op.id;
       new_vote.timestamp = op.timestamp;
       new_vote.vote = op.vote;

       _current_state->store_proposal_vote( new_vote );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_fire_operation( const fire_delegate_operation& op )
   {
       auto delegate_record = _current_state->get_account_record( op.delegate_id );
       FC_ASSERT( !!delegate_record && delegate_record->is_delegate() );
       switch( (fire_delegate_operation::reason_type)op.reason )
       {
          case fire_delegate_operation::multiple_blocks_signed:
          {
             auto proof = fc::raw::unpack<multiple_block_proof>( op.data );
             FC_ASSERT( proof.first.id() != proof.second.id() );
             FC_ASSERT( proof.first.timestamp == proof.second.timestamp )
             FC_ASSERT( proof.first.signee() == proof.second.signee() )
             FC_ASSERT( proof.first.validate_signee( delegate_record->active_key() ) );

             // then fire the delegate
             delegate_record->adjust_votes_against( BTS_BLOCKCHAIN_FIRE_VOTES );
             _current_state->store_account_record( *delegate_record );
             break;
          }
          case fire_delegate_operation::invalid_testimony:
          {
             auto testimony = fc::raw::unpack<signed_delegate_testimony>( op.data );
             FC_ASSERT( address(testimony.signee()) == delegate_record->active_address() );
             auto trx_loc = _current_state->get_transaction_location( testimony.transaction_id );

             if( testimony.valid || !!trx_loc  )
             {
                // then fire the delegate
                delegate_record->adjust_votes_against( BTS_BLOCKCHAIN_FIRE_VOTES );
                _current_state->store_account_record( *delegate_record );
             }

             break;
          }
       }
   }
   bool transaction_evaluation_state::check_signature( const address& a )const
   {
      return  signed_keys.find( a ) != signed_keys.end();
   }

   void transaction_evaluation_state::add_required_signature( const address& a )
   {
      required_keys.insert(a);
   }

   void transaction_evaluation_state::add_required_deposit( const address& owner_key, const asset& amount )
   {
      FC_ASSERT( !!trx.delegate_id );
      balance_id_type balance_id = withdraw_condition( 
                                       withdraw_with_signature( owner_key ), 
                                       amount.asset_id, *trx.delegate_id ).get_address();

      auto itr = required_deposits.find( balance_id );
      if( itr == required_deposits.end() )
      {
         required_deposits[balance_id] = amount;
      }
      else
      {
         required_deposits[balance_id] += amount;
      }
   }
   void transaction_evaluation_state::evaluate_withdraw_pay( const withdraw_pay_operation& op )
   { try {
      if( op.amount <= 0 ) fail( BTS_NEGATIVE_WITHDRAW, fc::variant(op) );

      auto cur_record = _current_state->get_account_record( op.account_id );
      if( !cur_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
      if( cur_record->is_retracted() ) fail( BTS_NAME_RETRACTED, fc::variant(op) );
      FC_ASSERT( cur_record->is_delegate() );

      add_required_signature( cur_record->active_address() );

      FC_ASSERT( cur_record->delegate_info->pay_balance >= op.amount );
      cur_record->delegate_info->pay_balance -= op.amount;

      _current_state->store_account_record( *cur_record );
      add_balance( asset(op.amount, 0) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_withdraw( const withdraw_operation& op )
   { try {
      if( op.amount <= 0 ) fail( BTS_NEGATIVE_WITHDRAW, fc::variant(op) );
      obalance_record arec = _current_state->get_balance_record( op.balance_id );
      if( !arec.valid() )
      {
         fail( BTS_UNDEFINED_ADDRESS, fc::variant(op) );
         FC_ASSERT( !"Evaluating withdraw, but unable to find balance record", "", ("op",op) );
      }

      switch( (withdraw_condition_types)arec->condition.type )
      {
         case withdraw_signature_type:  
         {
            add_required_signature( arec->condition.as<withdraw_with_signature>().owner );
            break;
         }
         case withdraw_by_account_type:  
         {
            add_required_signature( arec->condition.as<withdraw_by_account>().owner );
            break;
         }

         case withdraw_multi_sig_type:
         {
            auto multi_sig = arec->condition.as<withdraw_with_multi_sig>();
            uint32_t valid_signatures = 0;
            for( auto sig : multi_sig.owners )
               valid_signatures += check_signature( sig );
            if( valid_signatures < multi_sig.required )
               fail( BTS_MISSING_SIGNATURE, fc::variant(op) );
            break;
         }

         case withdraw_password_type:
         {
            auto pass = arec->condition.as<withdraw_with_password>();
            if( pass.timeout < _current_state->now() )
               check_signature( pass.payor );
            else 
            {
               check_signature( pass.payee );
               auto input_password_hash = fc::ripemd160::hash( op.claim_input_data.data(), 
                                                              op.claim_input_data.size() );

               if( pass.password_hash != input_password_hash )
               {
                  fail( BTS_INVALID_PASSWORD, fc::variant(op) );
               }
            }
            break;
         }

         case withdraw_option_type:
         {
            auto option = arec->condition.as<withdraw_option>();

            if( _current_state->now() > option.date )
            {
               add_required_signature( option.optionor );
            }
            else // the option hasn't expired
            {
               add_required_signature( option.optionee );

               auto pay_amount = asset( op.amount, arec->condition.asset_id ) * option.strike_price;
               add_required_deposit( option.optionee, pay_amount ); 
            }
            break;
         }
         case withdraw_null_type:
            fail( BTS_INVALID_WITHDRAW_CONDITION, fc::variant(op) );
            break;
      //   default:
      }
      // update delegate vote on withdrawn account..

      arec->balance -= op.amount;
      arec->last_update = _current_state->now();
      add_balance( asset(op.amount, arec->condition.asset_id) );

      if( arec->condition.asset_id == 0 ) 
         sub_vote( arec->condition.delegate_id, op.amount );

      _current_state->store_balance_record( *arec );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::fail( bts_error_code error_code, const fc::variant& data )
   {
      validation_error_code = error_code;
      FC_ASSERT( !error_code, "Error evaluating transaction ${error_code}", ("error_code",error_code)("data",data) );
   }

   void transaction_evaluation_state::evaluate_deposit( const deposit_operation& op )
   { try {
       if( op.amount <= 0 ) fail( BTS_NEGATIVE_DEPOSIT, fc::variant(op) );
       auto deposit_balance_id = op.balance_id();
       auto delegate_record = _current_state->get_account_record( op.condition.delegate_id );
       if( !delegate_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
       if( !delegate_record->is_delegate() ) fail( BTS_INVALID_DELEGATE_ID, fc::variant(op) );

       auto cur_record = _current_state->get_balance_record( deposit_balance_id );
       if( !cur_record )
       {
          cur_record = balance_record( op.condition );
       }
       cur_record->last_update   = _current_state->now();
       cur_record->balance       += op.amount;

       sub_balance( deposit_balance_id, asset(op.amount, cur_record->condition.asset_id) );
       
       if( cur_record->condition.asset_id == 0 ) 
          add_vote( cur_record->condition.delegate_id, op.amount );

       _current_state->store_balance_record( *cur_record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }



   void transaction_evaluation_state::add_vote( account_id_type delegate_id, share_type amount )
   {
      auto account_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[account_id].votes_for += amount;
      else if( delegate_id < 0 )
         net_delegate_votes[account_id].votes_against += amount;
   }
   void transaction_evaluation_state::sub_vote( account_id_type delegate_id, share_type amount )
   {
      auto account_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[account_id].votes_for -= amount;
      else if( delegate_id < 0 )
         net_delegate_votes[account_id].votes_against -= amount;
   }

   /**
    *  
    */
   void transaction_evaluation_state::sub_balance( const balance_id_type& balance_id, const asset& amount )
   {
      auto provided_deposit_itr = provided_deposits.find( balance_id );
      if( provided_deposit_itr == provided_deposits.end() )
      {
         provided_deposits[balance_id] = amount;
      }
      else
      {
         provided_deposit_itr->second += amount;
      }

      auto deposit_itr = deposits.find(amount.asset_id);
      if( deposit_itr == deposits.end()  ) 
      {
         deposits[amount.asset_id] = amount;
      }
      else
      {
         deposit_itr->second += amount.amount;
      }

      auto balance_itr = balance.find(amount.asset_id);
      if( balance_itr != balance.end()  )
      {
          balance_itr->second -= amount.amount;
      }
      else
      {
          balance[amount.asset_id] =  -amount.amount;
      }
   }

   void transaction_evaluation_state::add_balance( const asset& amount )
   {
      auto withdraw_itr = withdraws.find( amount.asset_id );
      if( withdraw_itr == withdraws.end() )
         withdraws[amount.asset_id] = amount.amount;
      else
         withdraw_itr->second += amount.amount;

      auto balance_itr = balance.find( amount.asset_id );
      if( balance_itr == balance.end() )
         balance[amount.asset_id] = amount.amount;
      else
         balance_itr->second += amount.amount;
   }

   void transaction_evaluation_state::evaluate_register_account( const register_account_operation& op )
   { try {
      FC_ASSERT( account_record::is_valid_name( op.name ) );

      auto cur_record = _current_state->get_account_record( op.name );
      if( cur_record.valid() && ((fc::time_point(cur_record->last_update) + one_year) > fc::time_point(_current_state->now())) ) 
         fail( BTS_NAME_ALREADY_REGISTERED, fc::variant(op) );

      auto account_with_same_key = _current_state->get_account_record( address(op.active_key) );
      if( account_with_same_key.valid() )
         FC_ASSERT( !"Account already registered using the requested key" );

      account_with_same_key = _current_state->get_account_record( address(op.owner_key) );
      if( account_with_same_key.valid() )
         FC_ASSERT( !"Account already registered using the requested key" );

      account_record new_record;
      new_record.id            = _current_state->new_account_id();
      new_record.name          = op.name;
      new_record.public_data     = op.public_data;
      new_record.owner_key     = op.owner_key;
      new_record.set_active_key( _current_state->now(), op.active_key );
      new_record.last_update   = _current_state->now();
      new_record.registration_date = _current_state->now();
      if (op.is_delegate)
      {
          new_record.delegate_info = delegate_stats();
      }

      cur_record = _current_state->get_account_record( new_record.id );
      FC_ASSERT( !cur_record );

      if( op.is_delegate )
      {
         // pay fee
         sub_balance( balance_id_type(), asset(_current_state->get_delegate_registration_fee()) );
      }

      _current_state->store_account_record( new_record );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_update_account( const update_account_operation& op )
   { try {
      auto cur_record = _current_state->get_account_record( op.account_id );
      if( !cur_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
      if( cur_record->is_retracted() ) fail( BTS_NAME_RETRACTED, fc::variant(op) );

      if( !!op.active_key && *op.active_key != cur_record->active_key() )
         add_required_signature( address(cur_record->owner_key) );
      else
         add_required_signature( cur_record->active_address() );

      if( !!op.public_data )
      {
         cur_record->public_data  = *op.public_data;
      }

      cur_record->last_update   = _current_state->now();

      if( op.active_key.valid() )
      {
         cur_record->set_active_key( _current_state->now(), *op.active_key );
         auto account_with_same_key = _current_state->get_account_record( address(*op.active_key) );
         if( account_with_same_key.valid() )
            FC_ASSERT( !"Account already registered using the requested key" );
      }

      if ( !cur_record->is_delegate() && op.is_delegate )
      {
         cur_record->delegate_info = delegate_stats();
      }

      _current_state->store_account_record( *cur_record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_create_asset( const create_asset_operation& op )
   { try {
      if( op.maximum_share_supply <= 0 ) fail( BTS_NEGATIVE_ISSUE, fc::variant(op) );
      auto cur_record = _current_state->get_asset_record( op.symbol );
      if( cur_record.valid() ) fail( BTS_ASSET_ALREADY_REGISTERED, fc::variant(op) );

      if( op.issuer_account_id != asset_record::market_issued_asset  )
      {
         auto issuer_account_record = _current_state->get_account_record( op.issuer_account_id );
         if( op.issuer_account_id > 0 && !issuer_account_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
         add_required_signature(issuer_account_record->active_address());
      }

      sub_balance( balance_id_type(), asset(_current_state->get_asset_registration_fee() , 0) );

      asset_record new_record;
      new_record.id                    = _current_state->new_asset_id();
      new_record.symbol                = op.symbol;
      new_record.name                  = op.name;
      new_record.description           = op.description;
      new_record.public_data           = op.public_data;
      new_record.issuer_account_id      = op.issuer_account_id;
      new_record.current_share_supply  = 0;
      new_record.maximum_share_supply  = op.maximum_share_supply;
      new_record.collected_fees        = 0;
      new_record.registration_date     = _current_state->now();

      _current_state->store_asset_record( new_record );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_update_asset( const update_asset_operation& op )
   { try {
      auto cur_record = _current_state->get_asset_record( op.asset_id );
      if( !cur_record ) fail( BTS_INVALID_ASSET_ID, fc::variant(op) );
      auto issuer_account_record = _current_state->get_account_record( cur_record->issuer_account_id );
      if( !issuer_account_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );

      add_required_signature(issuer_account_record->active_address());  

      if( op.issuer_account_id != cur_record->issuer_account_id )
      {
          auto new_issuer_account_record = _current_state->get_account_record( op.issuer_account_id );
          if( !new_issuer_account_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
          add_required_signature(new_issuer_account_record->active_address()); 
      }

      cur_record->description    = op.description;
      cur_record->public_data      = op.public_data;
      cur_record->issuer_account_id = op.issuer_account_id;
      cur_record->last_update    = _current_state->now();

      _current_state->store_asset_record( *cur_record );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_issue_asset( const issue_asset_operation& op )
   { try {
      FC_ASSERT( op.amount > 0, "amount: ${amount}", ("amount",op.amount) );
      auto cur_record = _current_state->get_asset_record( op.amount.asset_id );
      if( !cur_record ) 
         fail( BTS_INVALID_ASSET_ID, fc::variant(op) );

      auto issuer_account_record = _current_state->get_account_record( cur_record->issuer_account_id );
      if( !issuer_account_record ) 
         fail( BTS_INVALID_NAME_ID, fc::variant(op) );

      add_required_signature( issuer_account_record->active_address() );

      if( !cur_record->can_issue( op.amount ) )
         fail( BTS_INSUFFICIENT_FUNDS, fc::variant(op) );
    
      cur_record->current_share_supply += op.amount.amount;
      
      // TODO: To be checked, issued asset is not for pay fee, nor related to balance i/o, issued out from void.
      //add_balance( op.amount );

      _current_state->store_asset_record( *cur_record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_bid( const bid_operation& op )
   { try {
      FC_ASSERT( op.amount != 0 );

      market_index_key bid_index(op.bid_price,op.owner);
      auto cur_bid  = _current_state->get_bid_record( bid_index );
      if( !cur_bid )
      {  // then this is a new bid
         cur_bid = order_record( 0, op.delegate_id );
      }

      if( op.get_amount().asset_id == BASE_ASSET_ID )
      {
         auto delegate_record = _current_state->get_account_record( op.delegate_id );
         FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
         if( cur_bid->balance )
            sub_vote( cur_bid->delegate_id, cur_bid->balance );

         cur_bid->delegate_id = op.delegate_id;
         cur_bid->balance      += op.amount;
         FC_ASSERT( cur_bid->balance >= 0 );

         if( cur_bid->balance )
            add_vote( cur_bid->delegate_id, cur_bid->balance );
      }
      else
      {
         cur_bid->balance      += op.amount;
         FC_ASSERT( cur_bid->balance > 0 );
      }

      if( op.amount < 0 ) // we are withdrawing part or all of the bid (canceling the bid)
      { // this is effectively a withdraw and move to deposit...
         add_balance( -op.get_amount() );  
         add_required_signature( op.owner );
      }
      else if( op.amount > 0 ) // we are adding more value to the bid (increasing the amount, but not price)
      { // this is similar to a deposit
         sub_balance( balance_id_type(), op.get_amount() );
      }

      _current_state->store_bid_record( bid_index, *cur_bid );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_ask( const ask_operation& op )
   {
   }
   void transaction_evaluation_state::evaluate_short( const short_operation& op )
   {
   }
   void transaction_evaluation_state::evaluate_cover( const cover_operation& op )
   {
   }


   void transaction::withdraw( const balance_id_type& account, share_type amount )
   { try {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.push_back( withdraw_operation( account, amount ) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account",account)("amount",amount) ) }

   void transaction::deposit( const address& owner, const asset& amount, account_id_type delegate_id )
   {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.push_back( deposit_operation( owner, amount, delegate_id ) );
   }

   void transaction::deposit_to_account( fc::ecc::public_key receiver_key,
                                      asset amount,
                                      fc::ecc::private_key from_key,
                                      const std::string& memo_message,
                                      account_id_type delegate_id,
                                      memo_flags_enum memo_type )
   {
      fc::ecc::private_key one_time_private_key = fc::ecc::private_key::generate();

      withdraw_by_account by_account;
      by_account.encrypt_memo_data( one_time_private_key,
                                 receiver_key,
                                 from_key,
                                 memo_message,
                                 memo_type );

      deposit_operation op;
      op.amount = amount.amount;
      op.condition = withdraw_condition( by_account, amount.asset_id, delegate_id );

      operations.push_back( op );
   }


   void transaction::register_account( const std::string& name, 
                                   const fc::variant& public_data, 
                                   const public_key_type& master, 
                                   const public_key_type& active, bool as_delegate  )
   {
      operations.push_back( register_account_operation( name, public_data, master, active, as_delegate ) );
   }
   void transaction::update_account( account_id_type account_id, 
                                  const fc::optional<fc::variant>& public_data, 
                                  const fc::optional<public_key_type>& active, bool as_delegate   )
   {
      update_account_operation op;
      op.account_id = account_id;
      op.public_data = public_data;
      op.active_key = active;
      op.is_delegate = as_delegate;
      operations.push_back( op );
   }

   void transaction::submit_proposal(account_id_type delegate_id,
                                     const std::string& subject,
                                     const std::string& body,
                                     const std::string& proposal_type,
                                     const fc::variant& public_data)
   {
     submit_proposal_operation op;
     op.submitting_delegate_id = delegate_id;
     op.submission_date = fc::time_point::now();
     op.subject = subject;
     op.body = body;
     op.proposal_type = proposal_type;
     op.data = public_data;
     operations.push_back(op);
   }

   void transaction::vote_proposal(proposal_id_type proposal_id,
                                   account_id_type voter_id,
                                   uint8_t vote)
   {
     vote_proposal_operation op;
     op.id.proposal_id = proposal_id;
     op.id.delegate_id = voter_id;
     op.timestamp = fc::time_point::now();
     op.vote = vote;
     operations.push_back(op);
   }


   share_type transaction_evaluation_state::get_fees( asset_id_type id )const
   {
      auto itr = balance.find(id);
      if( itr != balance.end() )
         return itr->second;
      return 0;
   }
   void transaction::create_asset( const std::string& symbol, 
                                   const std::string& name, 
                                   const std::string& description,
                                   const fc::variant& data,
                                   account_id_type issuer_id,
                                   share_type   max_share_supply )
   {
      FC_ASSERT( max_share_supply > 0 );
      create_asset_operation op;
      op.symbol = symbol;
      op.name = name;
      op.description = description;
      op.public_data = data;
      op.issuer_account_id = issuer_id;
      op.maximum_share_supply = max_share_supply;
      operations.push_back( op );
   }

   void transaction::issue( const asset& amount_to_issue )
   {
      operations.push_back( issue_asset_operation( amount_to_issue ) );
   }

} } // bts::blockchain 
