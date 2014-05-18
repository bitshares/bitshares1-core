#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/error_codes.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {

   static const fc::microseconds one_year = fc::seconds( 60*60*24*365 );

   digest_type transaction::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack(enc,*this);
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
   void signed_transaction::sign( const fc::ecc::private_key& signer )
   {
      signatures.push_back( signer.sign_compact( digest() ) );
   }

   transaction_evaluation_state::transaction_evaluation_state( const chain_interface_ptr& current_state )
   :_current_state( current_state )
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
      auto digest = trx_arg.digest();
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
         auto del_rec = _current_state->get_name_record( del_vote.first );
         FC_ASSERT( !!del_rec );
         del_rec->adjust_votes_for( del_vote.second.votes_for );
         del_rec->adjust_votes_against( del_vote.second.votes_against );
         if( del_rec->votes_for() > max_votes || del_rec->votes_against() > max_votes )
            fail( BTS_DELEGATE_MAX_VOTE_LIMIT, fc::variant() );
         _current_state->store_name_record( *del_rec );
      }
   }

   void transaction_evaluation_state::evaluate_operation( const operation& op )
   {
      switch( (operation_type_enum)op.type  )
      {
         case withdraw_op_type:
            evaluate_withdraw( op.as<withdraw_operation>() );
            break;
         case deposit_op_type:
            evaluate_deposit( op.as<deposit_operation>() );
            break;
         case reserve_name_op_type:
            evaluate_reserve_name( op.as<reserve_name_operation>() );
            break;
         case update_name_op_type:
            evaluate_update_name( op.as<update_name_operation>() );
            break;
         case create_asset_op_type:
            evaluate_create_asset( op.as<create_asset_operation>() );
            break;
         case update_asset_op_type:
            evaluate_update_asset( op.as<update_asset_operation>() );
            break;
         case issue_asset_op_type:
            evaluate_issue_asset( op.as<issue_asset_operation>() );
            break;
         case null_op_type:
            break;
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

   void transaction_evaluation_state::evaluate_withdraw( const withdraw_operation& op )
   { try {
      obalance_record arec = _current_state->get_balance_record( op.balance_id );
      if( !arec ) fail( BTS_UNDEFINED_ADDRESS, fc::variant(op) );

      switch( (withdraw_condition_types)arec->condition.type )
      {
         case withdraw_signature_type:  
         {
            add_required_signature( arec->condition.as<withdraw_with_signature>().owner );
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
            uint32_t count = 0;
            count += check_signature( pass.payor );
            count += check_signature( pass.payee );
            if( count < 2 && op.claim_input_data.size() ) 
               count += pass.password_hash == fc::ripemd160::hash( op.claim_input_data.data(), op.claim_input_data.size() );
            if( count != 2 )
               fail( BTS_MISSING_SIGNATURE, fc::variant(op) );
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
         default:
            fail( BTS_INVALID_WITHDRAW_CONDITION, fc::variant(op) );
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
       auto deposit_balance_id = op.balance_id();
       auto delegate_record = _current_state->get_name_record( op.condition.delegate_id );
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



   void transaction_evaluation_state::add_vote( name_id_type delegate_id, share_type amount )
   {
      auto name_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[name_id].votes_for += amount;
      else if( delegate_id < 0 )
         net_delegate_votes[name_id].votes_against += amount;
   }
   void transaction_evaluation_state::sub_vote( name_id_type delegate_id, share_type amount )
   {
      auto name_id = abs(delegate_id);
      if( delegate_id > 0 )
         net_delegate_votes[name_id].votes_for -= amount;
      else if( delegate_id < 0 )
         net_delegate_votes[name_id].votes_against -= amount;
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

   void transaction_evaluation_state::evaluate_reserve_name( const reserve_name_operation& op )
   { try {
      FC_ASSERT( name_record::is_valid_name( op.name ) );
      FC_ASSERT( name_record::is_valid_json( op.json_data ) );
      FC_ASSERT( op.json_data.size() < BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE );

      auto cur_record = _current_state->get_name_record( op.name );
      if( cur_record && ((fc::time_point(cur_record->last_update) + one_year) > fc::time_point(_current_state->now())) ) 
         fail( BTS_NAME_ALREADY_REGISTERED, fc::variant(op) );

      name_record new_record;
      new_record.id            = _current_state->new_name_id();
      new_record.name          = op.name;
      new_record.json_data     = op.json_data;
      new_record.owner_key     = op.owner_key;
      new_record.active_key    = op.active_key;
      new_record.last_update   = _current_state->now();
      new_record.registration_date = _current_state->now();
      new_record.delegate_info = delegate_stats();

      cur_record = _current_state->get_name_record( new_record.id );
      FC_ASSERT( !cur_record );

      if( op.is_delegate )
      {
         // pay fee
         sub_balance( balance_id_type(), asset(_current_state->get_delegate_registration_fee()) );
      }

      _current_state->store_name_record( new_record );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_update_name( const update_name_operation& op )
   { try {
      auto cur_record = _current_state->get_name_record( op.name_id );
      if( !cur_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
      if( cur_record->is_retracted() ) fail( BTS_NAME_RETRACTED, fc::variant(op) );

      if( !!op.active_key && *op.active_key != cur_record->active_key )
         add_required_signature( cur_record->owner_key );
      else
         add_required_signature( cur_record->active_key );

      if( !!op.json_data )
      {
         FC_ASSERT( name_record::is_valid_json( *op.json_data ) );
         FC_ASSERT( op.json_data->size() < BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE );
         cur_record->json_data  = *op.json_data;
      }

      cur_record->last_update   = _current_state->now();

      if( !!op.active_key )
         cur_record->active_key = *op.active_key;

      _current_state->store_name_record( *cur_record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_create_asset( const create_asset_operation& op )
   { try {
      auto cur_record = _current_state->get_asset_record( op.symbol );
      if( cur_record ) fail( BTS_ASSET_ALREADY_REGISTERED, fc::variant(op) );
      auto issuer_name_record = _current_state->get_name_record( op.issuer_name_id );
      if( !issuer_name_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );

      add_required_signature(issuer_name_record->active_key);

      sub_balance( balance_id_type(), asset(_current_state->get_asset_registration_fee() , 0) );

      // TODO: verify that the json_data is properly formatted
      asset_record new_record;
      new_record.id                    = _current_state->new_asset_id();
      new_record.symbol                = op.symbol;
      new_record.name                  = op.name;
      new_record.description           = op.description;
      new_record.json_data             = op.json_data;
      new_record.issuer_name_id        = op.issuer_name_id;
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
      auto issuer_name_record = _current_state->get_name_record( cur_record->issuer_name_id );
      if( !issuer_name_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );

      add_required_signature(issuer_name_record->active_key);  

      if( op.issuer_name_id != cur_record->issuer_name_id )
      {
          auto new_issuer_name_record = _current_state->get_name_record( op.issuer_name_id );
          if( !new_issuer_name_record ) fail( BTS_INVALID_NAME_ID, fc::variant(op) );
          add_required_signature(new_issuer_name_record->active_key); 
      }

      cur_record->description    = op.description;
      cur_record->json_data      = op.json_data;
      cur_record->issuer_name_id = op.issuer_name_id;
      cur_record->last_update    = _current_state->now();

      _current_state->store_asset_record( *cur_record );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }

   void transaction_evaluation_state::evaluate_issue_asset( const issue_asset_operation& op )
   { try {
      auto cur_record = _current_state->get_asset_record( op.asset_id );
      if( !cur_record ) 
         fail( BTS_INVALID_ASSET_ID, fc::variant(op) );

      auto issuer_name_record = _current_state->get_name_record( cur_record->issuer_name_id );
      if( !issuer_name_record ) 
         fail( BTS_INVALID_NAME_ID, fc::variant(op) );

      add_required_signature( issuer_name_record->active_key );

      if( cur_record->available_shares() < op.amount )
         fail( BTS_INSUFFICIENT_FUNDS, fc::variant(op) );
    
      cur_record->current_share_supply += op.amount;
      add_balance( asset(op.amount, op.asset_id) );

      _current_state->store_asset_record( *cur_record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("op",op) ) }



   void transaction::withdraw( const balance_id_type& account, share_type amount )
   {
      operations.push_back( withdraw_operation( account, amount ) );
   }

   void transaction::deposit( const address& owner, const asset& amount, name_id_type delegate_id )
   {
      operations.push_back( deposit_operation( owner, amount, delegate_id ) );
   }
   void transaction::reserve_name( const std::string& name, 
                                   const std::string& json_data, 
                                   const public_key_type& master, 
                                   const public_key_type& active, bool as_delegate  )
   {
      operations.push_back( reserve_name_operation( name, json_data, master, active, as_delegate ) );
   }
   void transaction::update_name( name_id_type name_id, 
                                  const fc::optional<std::string>& json_data, 
                                  const fc::optional<public_key_type>& active, bool as_delegate   )
   {
      update_name_operation op;
      op.name_id = name_id;
      op.json_data = json_data;
      op.active_key = active;
      op.is_delegate = as_delegate;
      operations.push_back( op );
   }

   share_type transaction_evaluation_state::get_fees( asset_id_type id )const
   {
      auto itr = balance.find(id);
      if( itr != balance.end() )
         return itr->second;
      return 0;
   }

} } // bts::blockchain 
