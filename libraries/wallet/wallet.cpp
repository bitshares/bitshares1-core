#include <bts/wallet/wallet.hpp>
#include <bts/wallet/extended_address.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/import_bitcoin_wallet.hpp>
#include <unordered_map>
#include <map>
#include <fc/filesystem.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/crypto/aes.hpp>
#include <sstream>

#include <iostream>
#include <iomanip>
#include <algorithm>

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

namespace bts { namespace wallet {



   /**
    * this is the data that is stored on disk in an encrypted form protected by
    * the wallet password.
    */
   struct wallet_data
   {
       uint32_t                                                 version;
       uint32_t                                                 last_used_key;
       uint32_t                                                 last_scanned_block_num;
       std::unordered_map<address,std::string>                  recv_addresses;
       std::unordered_map<pts_address,address>                  recv_pts_addresses;
       std::unordered_map<address,std::string>                  send_addresses;

       /**
        *  If this wallet is a delegate wallet, these are the keys that it controls
        *  for producing blocks.
        */
       std::unordered_map<uint32_t,fc::ecc::private_key>        delegate_keys;
       std::unordered_set<uint32_t>                             trusted_delegates;
       std::unordered_set<uint32_t>                             distrusted_delegates;




       //std::vector<fc::ecc::private_key>                 keys;
       // an aes encrypted std::unordered_map<address,fc::ecc::private_key>
       std::vector<char>                                        encrypted_keys;
       std::vector<char>                                        encrypted_base_key;
       //
       std::unordered_map<transaction_id_type, transaction_state> transactions;
       std::map<output_index, trx_output>                         unspent_outputs;
       std::map<output_index, trx_output>                         spent_outputs;
       std::map<output_index, output_reference>                   output_index_to_ref;
       std::map<output_index, int32_t>                            votes;

       std::unordered_map<address,fc::ecc::private_key>    get_keys( const std::string& password )
       { try {
          //ilog( "get_keys with password '${pass}'", ("pass",password) );
          std::unordered_map<address, fc::ecc::private_key> keys;
          if( encrypted_keys.size() == 0 ) return keys;
          //ilog( "encrypted keys.size: ${s}", ("s",encrypted_keys.size() ) );

          auto plain_txt = fc::aes_decrypt( fc::sha512::hash( password.c_str(), password.size() ), encrypted_keys );
          //ilog( "plain_txt '${p}' size ${s}", ("p",plain_txt)("s",plain_txt.size()) );
          fc::datastream<const char*> ds(plain_txt.data(),plain_txt.size());
          fc::raw::unpack( ds, keys );
          return keys;
       } FC_RETHROW_EXCEPTIONS( warn, "" ) }

       extended_private_key                                     get_base_key( const std::string& password )
       {
          //ilog( "get_base_key  with password '${pass}'  encrypted_base_key ${ebk}", ("pass",password)("ebk",encrypted_base_key.size()) );
          extended_private_key base_key;
          if( encrypted_base_key.size() == 0 ) return base_key;

          auto plain_txt = fc::aes_decrypt( fc::sha512::hash( password.c_str(), password.size() ), encrypted_base_key );
          return fc::raw::unpack<extended_private_key>( plain_txt );
       }

       void set_keys( const std::unordered_map<address,fc::ecc::private_key>& k, const std::string& password )
       {
          auto plain_txt = fc::raw::pack( k );
          //ilog( "new_password '${p}'  plaint_txt '${pt}' size ${s}", ("p",password)("pt",plain_txt)("s",plain_txt.size()) );
          encrypted_keys = fc::aes_encrypt( fc::sha512::hash( password.c_str(), password.size() ), plain_txt );
          FC_ASSERT( k == get_keys(password) );
       }

       void change_password( const std::string& old_password, const std::string& new_password )
       {
          set_keys( get_keys( old_password ), new_password );
          set_base_key( get_base_key( old_password ), new_password );
       }

       void set_base_key( const extended_private_key& bk, const std::string& new_password )
       {
          auto plain_txt = fc::raw::pack( bk );
          //ilog( "new_password '${p}'  plaint_txt ${pt}", ("p",new_password)("pt",plain_txt) );
          encrypted_base_key = fc::aes_encrypt( fc::sha512::hash( new_password.c_str(), new_password.size() ), plain_txt );
          auto check = fc::aes_decrypt( fc::sha512::hash( new_password.c_str(), new_password.size() ), encrypted_base_key );
          FC_ASSERT( check == plain_txt );
       }

   };
} } // bts::wallet


FC_REFLECT( bts::wallet::wallet_data,
            (version)
            (last_used_key)
            (last_scanned_block_num)
            (recv_addresses)
            (recv_pts_addresses)
            (send_addresses)
            (encrypted_base_key)
            (encrypted_keys)
            (transactions)
            (unspent_outputs)
            (spent_outputs)
            (output_index_to_ref)
            (delegate_keys)
            (trusted_delegates)
            (distrusted_delegates)
          )

namespace bts { namespace wallet {

   output_index::operator std::string()const
   {
      std::stringstream ss;
      ss<<block_idx<<"."<<trx_idx<<"."<<output_idx;
      return ss.str();
   }

   namespace detail
   {
      class wallet_impl
      {
          public:
              wallet_impl():_stake(0),_is_open(false),_blockchain(nullptr){}
              std::string _wallet_base_password; // used for saving/loading the wallet
              std::string _wallet_key_password;  // used to access private keys

              fc::path                                                   _wallet_dat;
              fc::path                                                   _data_dir;
              wallet_data                                                _data;
              /** millishares per byte */
              uint64_t                                                   _current_fee_rate;
              uint64_t                                                   _stake;
              bool                                                       _is_open;

              //std::map<output_index, output_reference>                 _output_index_to_ref;
              // cached data for rapid lookup
              std::unordered_map<output_reference, output_index>         _output_ref_to_index;

              // keep sorted so we spend oldest first to maximize CDD
              //std::map<output_index, trx_output>                       _unspent_outputs;
              //std::map<output_index, trx_output>                       _spent_outputs;

              // maps address to private key index
              std::unordered_map<address,fc::ecc::private_key>             _my_keys;
              std::unordered_map<transaction_id_type,signed_transaction>   _id_to_signed_transaction;

              chain_database*                                              _blockchain;

              uint64_t get_fee_rate()
              {
                  return _current_fee_rate;
              }

              asset get_balance( asset_type balance_type )
              {
                   asset total_bal( static_cast<uint64_t>(0ull), balance_type);
                   std::vector<trx_input> inputs;
                   for( auto out : _data.unspent_outputs )
                   {
                      //ilog( "unspent outputs ${o}", ("o",*itr) );
                       if( out.second.claim_func == claim_by_signature && out.second.amount.unit == balance_type )
                       {
                           total_bal += out.second.amount; // TODO: apply interest earned
                       }
                       if( out.second.claim_func == claim_by_pts && out.second.amount.unit == balance_type )
                       {
                           total_bal += out.second.amount; // TODO: apply interest earned
                       }
                   }
                   return total_bal;
              }

              output_reference get_output_ref( const output_index& idx )
              {
                 auto itr = _data.output_index_to_ref.find( idx );
                 FC_ASSERT( itr != _data.output_index_to_ref.end() );
                 return itr->second;
              }

              address pts_to_bts_address( const pts_address& ptsaddr )
              {
                 auto pts_addr_itr = _data.recv_pts_addresses.find( ptsaddr );
                 FC_ASSERT( pts_addr_itr != _data.recv_pts_addresses.end() );
                 return pts_addr_itr->second;
              }

              std::vector<trx_input> collect_mining_input( asset& total_in, std::unordered_set<address>& req_sigs )
              {
                   uint64_t best_cdd = 0;
                   std::vector<trx_input> inputs;
                   inputs.resize(1);
                   address mine_addr;

                   for( auto out : _data.unspent_outputs  )
                   {
                       //ilog( "unspent outputs ${o}", ("o",*itr) );
                       if( out.second.claim_func == claim_by_signature && out.second.amount.unit == 0 )
                       {
                           auto cdd = out.second.amount.get_rounded_amount() * (_data.last_scanned_block_num - out.first.block_idx+1);
                           if( cdd > best_cdd )
                           {
                              inputs.back() = trx_input( get_output_ref(out.first) );
                              best_cdd = cdd;
                              mine_addr = out.second.as<claim_by_signature_output>().owner;
                              total_in = out.second.amount;
                           }
                       }
                       else if( out.second.claim_func == claim_by_pts && out.second.amount.unit == 0 )
                       {
                           auto cdd = out.second.amount.get_rounded_amount() * (_data.last_scanned_block_num - out.first.block_idx+1);
                           if( cdd > best_cdd )
                           {
                              inputs.back() = trx_input( get_output_ref(out.first) );
                              best_cdd = cdd;
                              total_in = out.second.amount;
                              mine_addr = pts_to_bts_address(out.second.as<claim_by_pts_output>().owner);
                           }
                       }
                   }
                   FC_ASSERT( best_cdd != 0 );
                   FC_ASSERT( mine_addr != address() );
                   ilog( "mine addr ${addr}", ("addr",mine_addr) );
                   req_sigs.insert( mine_addr );
                   return inputs;

              } // collect_mining_input


              /**
               *  Collect inputs that total to at least min_amnt.
               */
              std::vector<trx_input> collect_inputs( const asset& min_amnt, asset& total_in, std::unordered_set<address>& req_sigs )
              {
                   std::vector<trx_input> inputs;
                   for( auto out : _data.unspent_outputs )
                   {
                      //ilog( "unspent outputs ${o}", ("o",out) );
                       if( out.second.claim_func == claim_by_signature && out.second.amount.unit == min_amnt.unit )
                       {
                           inputs.push_back( trx_input( get_output_ref(out.first) ) );
                           total_in += out.second.amount;
                           req_sigs.insert( out.second.as<claim_by_signature_output>().owner );
                       //    ilog( "total in ${in}  min ${min}", ( "in",total_in)("min",min_amnt) );
                           if( total_in.get_rounded_amount() >= min_amnt.get_rounded_amount() )
                           {
                              return inputs;
                           }
                       }
                       else if( out.second.claim_func == claim_by_pts && out.second.amount.unit == min_amnt.unit )
                       {
                           inputs.push_back( trx_input( get_output_ref(out.first) ) );
                           total_in += out.second.amount;
                           req_sigs.insert( _data.recv_pts_addresses[out.second.as<claim_by_pts_output>().owner] );
                        //   ilog( "total in ${in}  min ${min}", ( "in",total_in)("min",min_amnt) );
                           if( total_in.get_rounded_amount() >= min_amnt.get_rounded_amount() )
                           {
                              return inputs;
                           }
                       }
                   }
                   FC_ASSERT( !"Unable to collect sufficient unspent inputs", "", ("min_amnt",min_amnt)("total_collected",total_in) );
              }

              /**
               *  This method should select the trusted delegate with the least votes or vote against
               *  any untrusted delegates that are in the top 200
               *
               *  @sa dpos_voting_algorithm
               */
              int32_t select_delegate_vote()
              { try {
                   FC_ASSERT( _data.trusted_delegates.size() > 0  );
                   // for now we will randomly select a trusted delegate to vote for... this
                   std::vector<uint32_t> trusted_del( _data.trusted_delegates.begin(), _data.trusted_delegates.end() );
                   return int32_t(trusted_del[rand()%trusted_del.size()]);
              } FC_RETHROW_EXCEPTIONS(warn, "") }


              /** completes a transaction signing it and logging it, this is different than wallet::sign_transaction which
               *  merely signs the transaction without checking anything else or storing the transaction.
               **/
              void sign_transaction( signed_transaction& trx, const std::unordered_set<address>& addresses, bool mark_output_as_used = true)
              { try {
                   trx.stake = _stake;
                   trx.vote  = select_delegate_vote();

                   for( auto itr = addresses.begin(); itr != addresses.end(); ++itr )
                   {
                      self->sign_transaction( trx, *itr );
                   }
                   if( mark_output_as_used )
                   {
                      for( auto itr = trx.inputs.begin(); itr != trx.inputs.end(); ++itr )
                      {
                          elog( "MARK AS SPENT ${B}", ("B",itr->output_ref) );
                          self->mark_as_spent( itr->output_ref );
                      }
                      _data.transactions[trx.id()].trx = trx;
                   }
              } FC_RETHROW_EXCEPTIONS( warn, "" ) }
              wallet* self;
      };
   } // namespace detail

   uint64_t wallet::get_fee_rate()
   {
      return my->get_fee_rate();
   }

   wallet::wallet()
   :my( new detail::wallet_impl() )
   {
      my->self = this;
   }

   wallet::~wallet()
   {
      try {
        save();
      } catch ( const fc::exception& e )
      {
         wlog( "unhandled exception while saving wallet ${e}", ("e",e.to_detail_string()) );
      }
   }

   void wallet::open( const fc::path& wallet_dat, const fc::string& password )
   {
       try {
           my->_wallet_dat           = wallet_dat;
           my->_wallet_base_password = password;

           FC_ASSERT( fc::exists( wallet_dat ), "", ("wallet_dat",wallet_dat) )

           if( password == std::string() )
           {
               my->_data = fc::json::from_file<bts::wallet::wallet_data>( wallet_dat );
           }
           else
           {
               std::vector<char> plain_txt = aes_load( wallet_dat, fc::sha512::hash( password.c_str(), password.size() ) );
               FC_ASSERT( plain_txt.size() > 0 );
               std::string str( plain_txt.begin(), plain_txt.end() );
               my->_data = fc::json::from_string(str).as<wallet_data>();
           }

           for( auto item : my->_data.output_index_to_ref )
               my->_output_ref_to_index[item.second] = item.first;
           my->_is_open = true;
       }catch( fc::exception& er ) {
           my->_is_open = false;
           FC_RETHROW_EXCEPTION( er, warn, "unable to load ${wal}", ("wal",wallet_dat) );
       } catch( const std::exception& e ) {
           my->_is_open = false;
           throw  fc::std_exception(
               FC_LOG_MESSAGE( warn, "unable to load ${wal}", ("wal",wallet_dat) ),
               std::current_exception(),
               e.what() ) ;
       } catch( ... ) {
           my->_is_open = false;
           throw fc::unhandled_exception(
               FC_LOG_MESSAGE( warn, "unable to load ${wal}", ("wal",wallet_dat)),
               std::current_exception() );
       }
   }

   void wallet::set_data_directory( const fc::path& dir )
   {
      my->_data_dir = dir;
      my->_wallet_dat = dir / "wallet.bts";
   }
   fc::path wallet::get_wallet_file()const
   {
      return my->_wallet_dat;
   }

   void wallet::create( const fc::path& wallet_dat, const fc::string& base_password, const fc::string& key_password, bool is_brain )
   { try {
      FC_ASSERT( !fc::exists( wallet_dat ), "", ("wallet_dat",wallet_dat) );
      FC_ASSERT( key_password.size() >= 8 );

      my->_data = wallet_data();

      my->_wallet_dat = wallet_dat;
      my->_wallet_base_password = base_password;
      my->_wallet_key_password  = key_password;
      my->_is_open = false;

      for( uint32_t i = 1; i < 101; ++i )
      {
        my->_data.trusted_delegates.insert(i);
      }

      if( is_brain )
      {
         FC_ASSERT( base_password.size() >= 8 );
         my->_data.set_base_key( extended_private_key( fc::sha256::hash( key_password.c_str(), key_password.size() ),
                                                             fc::sha256::hash( base_password.c_str(), base_password.size() ) ), key_password );
      }
      else
      {
         my->_data.set_base_key( extended_private_key( fc::ecc::private_key::generate().get_secret(),
                                                       fc::ecc::private_key::generate().get_secret() ), key_password );
      }
      my->_data.set_keys( std::unordered_map<address,fc::ecc::private_key>(), key_password );
      save();
      my->_is_open = true;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create wallet ${wal}", ("wal",wallet_dat) ) }

  bool wallet::is_open() const
  {
    return my->_is_open;
  }

   void wallet::backup_wallet( const fc::path& backup_path )
   { try {
      FC_ASSERT( !fc::exists( backup_path ) );
      auto tmp = my->_wallet_dat;
      my->_wallet_dat = backup_path;
      try {
        save();
        my->_wallet_dat = tmp;
      }
      catch ( ... )
      {
        my->_wallet_dat = tmp;
        throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "unable to backup to ${path}", ("path",backup_path) ) }

   /**
    *  @note no balances will show up unless you scan the chain after import... perhaps just scan the
    *  genesis block which is the only place where PTS and BTC addresses should be found.
    */
   void wallet::import_bitcoin_wallet( const fc::path& wallet_dat, const std::string& passphrase )
   { try {
      auto priv_keys = bts::import_bitcoin_wallet(  wallet_dat, passphrase );
   //   ilog( "keys: ${keys}", ("keys",priv_keys) );
      for( auto key : priv_keys )
      {
         auto btc_addr = pts_address( key.get_public_key(), false, 0 );
         import_key( key, std::string( btc_addr ) );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to import bitcoin wallet ${wallet_dat}", ("wallet_dat",wallet_dat) ) }


   void wallet::save()
   { try {
      ilog( "saving wallet\n" );
      if(!my->_is_open)
          return;

      auto wallet_json = fc::json::to_pretty_string( my->_data );
      std::vector<char> data( wallet_json.begin(), wallet_json.end() );

      if( fc::exists( my->_wallet_dat ) )
      {
        auto new_tmp = fc::unique_path();
        auto old_tmp = fc::unique_path();
        if( my->_wallet_base_password.size() )
        {
          fc::aes_save( new_tmp, fc::sha512::hash( my->_wallet_base_password.c_str(), my->_wallet_base_password.size() ), data );
        }
        else
        {
           fc::json::save_to_file( my->_data, new_tmp, true );
        }
        fc::rename( my->_wallet_dat, old_tmp );
        fc::rename( new_tmp, my->_wallet_dat );
        fc::remove( old_tmp );
      }
      else
      {
         if( my->_wallet_base_password.size() != 0 )
         {
            fc::aes_save( my->_wallet_dat, fc::sha512::hash( my->_wallet_base_password.c_str(), my->_wallet_base_password.size() ), data );
         }
         else
         {
            fc::json::save_to_file( my->_data, my->_wallet_dat, true );
         }
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to save wallet ${wallet}", ("wallet",my->_wallet_dat) ) }

   asset wallet::get_balance( asset_type t )
   {
      return my->get_balance(t);
   }

   address   wallet::import_key( const fc::ecc::private_key& key, const std::string& label )
   { try {
      FC_ASSERT( !is_locked() );
      auto keys = my->_data.get_keys( my->_wallet_key_password );
      auto addr = address(key.get_public_key());
      keys[addr] = key;
      my->_data.set_keys( keys, my->_wallet_key_password );
      my->_data.recv_addresses[addr] = label;

      my->_data.recv_pts_addresses[ pts_address( key.get_public_key() ) ]           = addr;
      my->_data.recv_pts_addresses[ pts_address( key.get_public_key(), true ) ]     = addr;
      my->_data.recv_pts_addresses[ pts_address( key.get_public_key(), false, 0 ) ] = addr;
      my->_data.recv_pts_addresses[ pts_address( key.get_public_key(), true, 0 ) ]  = addr;

      return addr;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to import private key" ) }

   fc::ecc::public_key   wallet::new_public_key( const std::string& label )
   { try {
      FC_ASSERT( !is_locked() );
      my->_data.last_used_key++;
      auto base_key = my->_data.get_base_key( my->_wallet_key_password );
      auto new_key = base_key.child( my->_data.last_used_key );
      import_key(new_key, label);
      return new_key.get_public_key();
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create new address with label '${label}'", ("label",label) ) }

   address   wallet::new_recv_address( const std::string& label )
   { try {
      return address( new_public_key( label ) );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create new address with label '${label}'", ("label",label) ) }

   void wallet::add_send_address( const address& addr, const std::string& label )
   { try {
      my->_data.send_addresses[addr] = label;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to add send address ${addr} with label ${label}", ("addr",addr)("label",label) ) }

   std::unordered_map<address,std::string> wallet::get_recv_addresses()const
   {
      return my->_data.recv_addresses;
   }
   std::unordered_map<address,std::string> wallet::get_send_addresses()const
   {
      return my->_data.send_addresses;
   }

   void                  wallet::set_fee_rate( uint64_t milli_shares_per_byte )
   {
      my->_current_fee_rate = milli_shares_per_byte;
   }

   void                  wallet::unlock_wallet( const std::string& key_password )
   { try {
      my->_data.get_base_key( key_password );
      my->_wallet_key_password = key_password;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to unlock wallet" ) }
   void                  wallet::lock_wallet()
   {
      my->_wallet_base_password = std::string();
   }
   bool   wallet::is_locked()const { return my->_wallet_key_password.size() == 0; }


   signed_transaction wallet::transfer( const asset& amnt, const address& to, const std::string& memo )
   { try {
       signed_transaction trx;
       trx.outputs.push_back(trx_output(claim_by_signature_output(to), amnt));

       return collect_inputs_and_sign(trx, amnt, memo);
   } FC_RETHROW_EXCEPTIONS( warn, "${amnt} to ${to}", ("amnt",amnt)("to",to) ) }

   signed_transaction wallet::register_delegate( const std::string& name, const fc::variant& data )
   {
      FC_ASSERT( claim_name_output::is_valid_name(name), "", ("name",name)  );
      signed_transaction trx;
      trx.outputs.push_back( trx_output( claim_name_output( name, data, my->_blockchain->get_new_delegate_id(), new_public_key("delegate: "+name) ), asset() ) );
      return collect_inputs_and_sign( trx, asset(), std::string() );
   }

   void wallet::mark_as_spent( const output_reference& r )
   {
     // wlog( "MARK SPENT ${s}", ("s",r) );
      auto ref_itr = my->_output_ref_to_index.find(r);
      if( ref_itr == my->_output_ref_to_index.end() )
      {
         return;
      }

      auto itr = my->_data.unspent_outputs.find(ref_itr->second);
      if( itr == my->_data.unspent_outputs.end() )
      {
          return;
      }
      my->_data.spent_outputs[ref_itr->second] = itr->second;
      my->_data.unspent_outputs.erase(ref_itr->second);
   }

   void wallet::sign_transaction( signed_transaction& trx, const address& addr )
   { try {
      ilog( "Sign ${trx}  ${addr}", ("trx",trx.id())("addr",addr));
      FC_ASSERT( my->_wallet_key_password.size() );
      auto keys = my->_data.get_keys( my->_wallet_key_password );
      auto priv_key_itr = keys.find(addr);
      FC_ASSERT( priv_key_itr != keys.end() );
      trx.sign( priv_key_itr->second );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to sign transaction ${trx} for ${addr}", ("trx",trx)("addr",addr) ) }

   void wallet::sign_transaction( signed_transaction& trx, const std::unordered_set<address>& addresses, bool mark_output_as_used )
   {
      return my->sign_transaction( trx, addresses, mark_output_as_used );
   }

   /** returns all transactions issued */
   std::unordered_map<transaction_id_type, transaction_state> wallet::get_transaction_history()const
   {
      return my->_data.transactions;
   }

   void wallet::scan_input( transaction_state& state, const output_reference& r, const output_index& idx )
   {
      auto itr = my->_data.unspent_outputs.find(idx);
      if( itr != my->_data.unspent_outputs.end() )
      {
         state.adjust_balance( itr->second.amount, -1 );
         return;
      }
      itr = my->_data.spent_outputs.find(idx);
      if( itr != my->_data.spent_outputs.end() )
      {
         state.adjust_balance( itr->second.amount, -1 );
         return;
      }
   }

   bool wallet::scan_transaction( transaction_state& state, uint32_t block_idx, uint32_t trx_idx )
   {
       bool found = false;
       for( uint32_t in_idx = 0; in_idx < state.trx.inputs.size(); ++in_idx )
       {
           auto output_ref = state.trx.inputs[in_idx].output_ref;
           auto ref_itr = my->_output_ref_to_index.find(output_ref);
           if( ref_itr == my->_output_ref_to_index.end() )
           {
              continue;
           }
           scan_input( state, output_ref, ref_itr->second);
           mark_as_spent( output_ref );
       }

       // for each output
       transaction_id_type trx_id = state.trx.id();
       for( uint32_t out_idx = 0; out_idx < state.trx.outputs.size(); ++out_idx )
       {
           const trx_output& out   = state.trx.outputs[out_idx];
           const output_reference  out_ref( trx_id,out_idx );
           const output_index      oidx( block_idx, trx_idx, out_idx );
           found |= scan_output( state, out, out_ref, oidx );
       }
       return found;
   }

   /**
    *  Scan the blockchain starting from_block_num until the head block, check every
    *  transaction for inputs or outputs accessable by this wallet.
    *
    *  @return true if a new input was found or output spent
    */
   bool wallet::scan_chain( chain_database& chain, uint32_t from_block_num, scan_progress_callback cb )
   { try {
       my->_blockchain = &chain;
       bool found = false;
       auto head_block_num = chain.head_block_num();
       if( head_block_num == uint32_t(-1) ) return false;

       // for each block
       for( uint32_t i = from_block_num; i <= head_block_num; ++i )
       {
          auto blk = chain.fetch_digest_block( i );
          // for each transaction
          for( uint32_t trx_idx = 0; trx_idx < blk.trx_ids.size(); ++trx_idx )
          {
              if( cb ) cb( i, head_block_num, trx_idx, blk.trx_ids.size() );

              auto trx = chain.fetch_trx( trx_num( i, trx_idx ) );

              transaction_state state;
              state.trx = trx;
              bool found_output = scan_transaction( state, i, trx_idx );
              if( found_output )
                 my->_data.transactions[trx.id()] = state;
              found |= found_output;
          }
          for( uint32_t trx_idx = 0; trx_idx < blk.deterministic_ids.size(); ++trx_idx )
          {
              transaction_state state;
              state.trx = chain.fetch_trx( trx_num( i, blk.trx_ids.size() + trx_idx ) );
              bool found_output = scan_transaction( state, i, trx_idx );
              if( found_output )
                 my->_data.transactions[state.trx.id()] = state;
              found |= found_output;
          }
       }
       set_fee_rate( chain.get_fee_rate() );
       my->_stake                       = chain.get_stake();
       my->_data.last_scanned_block_num = head_block_num;
       return found;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /* @brief Dumps info strings for this wallet's last N transactions
    */
   void wallet::dump_txs(chain_database& db, uint32_t count)
   {
       auto txs = get_transaction_history();
       uint32_t i = 0;
       for (auto tx : txs)
       {
            if (i == count) break;
            i++;
            std::cerr << get_tx_info_string(db, tx.second.trx) << "\n";
       }
   }
   /* @brief Dump this wallet's subset of the blockchain's unspent transaction output set
    */
   void wallet::dump_utxo_set()
   {
       std::cerr<<"===========================================================\n";
       std::cerr<<"Unspent Outputs: \n";
       for( auto out : my->_data.unspent_outputs )
       {
          std::cerr<<std::setw(13)<<std::string(out.first)<<"]  ";
          dump_output(out.second);
          std::cerr << " delegate vote: " << my->_data.votes[out.first] <<" \n";
       }
       std::cerr<<"===========================================================\n";
   }

   void wallet::dump_output( const trx_output& out )
   {
       std::cerr << get_output_info_string(out);
   }

   std::string wallet::get_tx_info_string(chain_database& db, const transaction& tx)
   {
       std::stringstream ss;
       asset sum_in, sum_out;

       ss << "Inputs:\n";
       for (auto in : tx.inputs)
       {
          auto out = db.fetch_output(in.output_ref);
          sum_in += out.amount;
          ss << "  " << get_input_info_string(db, in);
       }

       ss << "Outputs:\n";
       for (auto out : tx.outputs)
       {
          sum_out += out.amount;
          ss << "  " << get_output_info_string(out);
       }

       ss <<"\n"
       <<"Total in: " << sum_in.get_rounded_amount() << "\n"
       <<"Total out: " << sum_out.get_rounded_amount() << "\n"
       <<"Fee: " << (sum_in - sum_out).get_rounded_amount() << "\n";

       return ss.str();
   }

   std::string wallet::get_output_info_string(const trx_output& out)
   {
       std::stringstream ret;
       switch (out.claim_func)
       {
          case claim_by_pts:
             ret<<std::string(out.amount)<<" ";
             ret<<"claim_by_pts ";
             ret<< std::string(out.as<claim_by_pts_output>().owner);
             ret<<"\n";
             break;
          case claim_by_signature:
             ret<<std::string(out.amount)<<" ";
             ret<<"claim_by_signature ";
             ret<< std::string(out.as<claim_by_signature_output>().owner);
             ret<<"\n";
             break;
          case claim_name:
          {
             auto claim = out.as<claim_name_output>();
             ret<<std::string(out.amount)<<" ";
             ret<<"claim_name ";
             ret<< claim.name;
             ret<<"\tdelegate_id: ";
             ret<< claim.delegate_id;
             ret<<"\tkey: "<<std::string(address(claim.owner));
             ret<<"\n";
             break;
          }
          default:
             ret << "unknown output type skipped: " << out.claim_func << "\n";
       }
       return ret.str();
   }

   std::string wallet::get_input_info_string(chain_database& db, const trx_input& in)
   {
       auto out = db.fetch_output(in.output_ref);
       return get_output_info_string(out);
   }


   bool wallet::is_my_address( const address& a )const
   {
      return my->_data.recv_addresses.find(a)  != my->_data.recv_addresses.end();
   }
   bool wallet::is_my_address( const pts_address& a )const
   {
      if( my->_data.recv_pts_addresses.find(a)  == my->_data.recv_pts_addresses.end() )
         return false;
      ilog( "found my address ${a}", ("a",a) );
      return true;
   }

   bool wallet::scan_output( transaction_state& state, const trx_output& out, const output_reference& out_ref, const bts::wallet::output_index& oidx )
   { try {
      switch( out.claim_func )
      {
         case claim_by_pts:
         {
            if( is_my_address( out.as<claim_by_pts_output>().owner ) )
            {
                cache_output( state.trx.vote, out, out_ref, oidx );
                state.adjust_balance( out.amount, 1 );
                return true;
            }
            return false;
         }
         case claim_by_signature:
         {
            auto owner = out.as<claim_by_signature_output>().owner;
            if( is_my_address( owner ) )
            {
               cache_output( state.trx.vote, out, out_ref, oidx );
               state.to.push_back( owner );
               state.adjust_balance( out.amount, 1 );
               return true;
            }
            return false;
         }
         case claim_name:
         {
            auto claim = out.as<claim_name_output>();
            auto itr = my->_data.delegate_keys.find( claim.delegate_id );
            if( itr != my->_data.delegate_keys.end() )
            {
               FC_ASSERT( itr->second.get_public_key() == claim.owner );
               cache_output( state.trx.vote, out, out_ref, oidx );
               state.to.push_back( claim.owner );
               return true;
            }
            return false;
         }
         default:
            FC_ASSERT( !"Invalid Claim Type" );
      }
      return false;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }


   output_reference wallet::get_ref_from_output_idx( output_index idx)
   {
       return my->get_output_ref(idx); //_output_index_to_ref[idx];
   }

   void wallet::cache_output( int32_t vote, const trx_output& out, const output_reference& out_ref, const output_index& oidx )
   {
       my->_data.output_index_to_ref[oidx]  = out_ref;
       my->_output_ref_to_index[out_ref]    = oidx;
       my->_data.unspent_outputs[oidx]      = out;
       my->_data.votes[oidx]                = vote;
   }
   const std::map<output_index,trx_output>&  wallet::get_unspent_outputs()const
   {
       return my->_data.unspent_outputs;
   }
   std::vector<trx_input> wallet::collect_inputs( const asset& min_amnt, asset& total_in, std::unordered_set<address>& req_sigs )
   {
      return my->collect_inputs( min_amnt, total_in, req_sigs );
   }

   trx_block  wallet::generate_next_block( chain_database& db, const signed_transactions& in_trxs )
   {
      wlog( "generate next block ${trxs}", ("trxs",in_trxs) );
      set_fee_rate( db.get_fee_rate() );
      my->_stake   = db.get_stake();

      try {
         auto deterministic_trxs = db.generate_deterministic_transactions();

         trx_block result;
         std::vector<trx_stat>  stats;
         stats.reserve(in_trxs.size());

         for( uint32_t i = 0; i < in_trxs.size(); ++i )
         {
            try {
                // create a new block state to evaluate transactions in isolation to maximize fees
                auto block_state = db.get_transaction_validator()->create_block_state();
                trx_stat s;
                s.eval = db.get_transaction_validator()->evaluate( in_trxs[i], block_state ); //evaluate_signed_transaction( in_trxs[i] );
                ilog( "eval: ${eval}  size: ${size} get_fee_rate ${r}", ("eval",s.eval)("size",in_trxs[i].size())("r",get_fee_rate()) );

               // TODO: enforce fees
                if( s.eval.fees < (get_fee_rate() * in_trxs[i].size())/1000 )
                {
                  wlog( "ignoring transaction ${trx} because it doesn't pay minimum fee ${f}\n\n state: ${s}",
                        ("trx",in_trxs[i])("s",s.eval)("f", (get_fee_rate()*in_trxs[i].size())/1000) );
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


         // create new block state to reject transactions that conflict with transactions that
         // have already been included in the block.
         auto block_state = db.get_transaction_validator()->create_block_state();
         transaction_summary summary;
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
            try {
               summary += db.get_transaction_validator()->evaluate( trx, block_state );
               result.trxs.push_back(trx);
            }
            catch ( const fc::exception& e )
            {
               wlog( "caught exception that failed to pass validation: ${e}", ("e",e.to_detail_string() ) );
            }
         }
         auto head_block = db.get_head_block();

         result.block_num       = db.head_block_num() + 1;
         result.prev            = db.head_block_id();
         result.trx_mroot       = result.calculate_merkle_root(deterministic_trxs);
         result.next_fee        = result.calculate_next_fee( db.get_fee_rate(), result.block_size() );
         result.total_shares    = head_block.total_shares - summary.fees;
         result.timestamp       = db.get_pow_validator()->get_time();

         return result;
      } FC_RETHROW_EXCEPTIONS( warn, "error generating new block" );

   }

   void wallet::set_delegate_trust( uint32_t did, bool is_trusted )
   {
      if( is_trusted )
      {
         my->_data.trusted_delegates.insert(did);
         my->_data.distrusted_delegates.erase(did);
      }
      else
      {
         my->_data.distrusted_delegates.insert(did);
         my->_data.trusted_delegates.erase(did);
      }
   }

   void wallet::import_delegate( uint32_t did, const fc::ecc::private_key& k )
   {
      my->_data.delegate_keys[did] = k;
   }

signed_transaction wallet::collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                   std::unordered_set<address>& req_sigs, const address& change_addr)
{
    /* Save original transaction inputs and outputs */
    auto original_inputs   = trx.inputs;
    auto original_outputs  = trx.outputs;
    auto original_req_sigs = req_sigs;

    auto required_in = min_amnt;
    asset total_in;

    do
    { /* Start with original transaction */
        trx.inputs = original_inputs;
        trx.outputs = original_outputs;
        req_sigs = original_req_sigs;

        /* Collect necessary inputs */
        total_in = asset(required_in.unit);
        auto new_inputs = collect_inputs(required_in, total_in, req_sigs); /* Throws if insufficient funds */
        trx.inputs.insert(trx.inputs.end(), new_inputs.begin(), new_inputs.end());

        /* Return change */
        auto change_amt = total_in - required_in;
        trx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));

        /* Ensure fee is paid in base units */
        if (change_amt.unit != asset().unit)
            return collect_inputs_and_sign(trx, asset(), req_sigs, change_addr);

        /* Calculate fee required for signed transaction */
        trx.sigs.clear();
        sign_transaction(trx, req_sigs, false);
        auto fee = (get_fee_rate() * trx.size())/1000;
        ilog("required fee ${f} for bytes ${b} at rate ${r} milli-shares per byte", ("f", fee) ("b", trx.size()) ("r", get_fee_rate()));

        /* Calculate new minimum input amount */
        required_in += fee;
        if (total_in < required_in)
        {
            wlog("not enough to cover amount + fee... grabbing more..");
            continue;
        }

        /* Recalculate leftover change */
        change_amt = total_in - required_in;
        if (change_amt > asset())
            trx.outputs.back() = trx_output(claim_by_signature_output(change_addr), change_amt);
        else
            trx.outputs.pop_back();

    } while (total_in < required_in); /* Try again with the new minimum input amount if the fee ended up too high */

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, true);

    return trx;
}

signed_transaction wallet::collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                   std::unordered_set<address>& req_sigs, const std::string& memo)
{
    return collect_inputs_and_sign(trx, min_amnt, req_sigs, new_recv_address("Change: " + memo));
}

signed_transaction wallet::collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                   std::unordered_set<address>& req_sigs)
{
    return collect_inputs_and_sign(trx, min_amnt, req_sigs, new_recv_address("Change address"));
}

signed_transaction wallet::collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                   const std::string& memo)
{
    std::unordered_set<address> req_sigs;
    return collect_inputs_and_sign(trx, min_amnt, req_sigs, new_recv_address("Change: " + memo));
}

signed_transaction wallet::collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt)
{
    std::unordered_set<address> req_sigs;
    return collect_inputs_and_sign(trx, min_amnt, req_sigs, new_recv_address("Change address"));
}

} } // namespace bts::wallet
