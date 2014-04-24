#pragma once
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>

#include <functional>
#include <unordered_map>
#include <map>

namespace bts {

namespace blockchain { class chain_database; }

namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   struct output_index
   {
        output_index( uint32_t block_id = 0, uint16_t trx_id = 0, uint16_t output_id = 0)
        :block_idx(block_id),trx_idx(trx_id),output_idx(output_id){}

        friend bool operator < ( const output_index& a, const output_index& b )
        {
           if( a.block_idx == b.block_idx )
           {
              if( a.trx_idx == b.trx_idx )
              {
                 return a.output_idx < b.output_idx;
              }
              else
              {
                 return a.trx_idx < b.trx_idx;
              }
           }
           else
           {
              return a.block_idx < b.block_idx;
           }
        }
        friend bool operator == ( const output_index& a, const output_index& b )
        {
          return a.block_idx == b.block_idx && a.trx_idx == b.trx_idx && a.output_idx == b.output_idx;
        }
        operator std::string()const;

        uint32_t   block_idx;
        uint16_t   trx_idx;
        uint16_t   output_idx;
   };


   struct transaction_state
   {
      transaction_state():delta_balance(0),block_num(-1),valid(false){}
      signed_transaction   trx;
      std::vector<address> to;
      std::vector<address> from;
      std::string          memo;
      std::unordered_map<uint16_t,int64_t> delta_balance; /// unit vs amount
      uint32_t             block_num; // block that included it, -1 if not included
      bool                 valid;     // is this transaction currently valid if it is not confirmed...

      void adjust_balance( asset amnt, int64_t direction = 1 )
      {
         auto itr = delta_balance.find(amnt.unit);
         if( itr == delta_balance.end() )
         {
              delta_balance[amnt.unit] = amnt.get_rounded_amount()*direction;
         }
         else
         {
            itr->second += amnt.get_rounded_amount()*direction;
         }
      }
   };

   /** takes 4 parameters, current block, last block, current trx, last trx */
   typedef std::function<void(uint32_t,uint32_t,uint32_t,uint32_t)> scan_progress_callback;

   /**
    *  The wallet stores all signed_transactions that reference one of its
    *  addresses in the inputs or outputs section.  It also tracks all
    *  private keys, spend states, etc...
    */
   class wallet
   {
        public:
           wallet();
           ~wallet();

           void set_data_directory( const fc::path& dir );
           fc::path get_wallet_filename_for_user(const std::string& username) const;

           void import_delegate( uint32_t did, const fc::ecc::private_key& k );
           void set_delegate_trust( uint32_t did,  bool is_trusted );

           signed_transaction register_delegate( const std::string& n, const fc::variant& v );

           void open( const fc::path& wallet_file, const std::string& password );
           bool close();
           void create( const fc::path& wallet_file, const std::string& base_pass, const std::string& key_pass, bool is_brain = false );
           bool is_open() const;

           void save();
           void backup_wallet( const fc::path& backup_path );

           void import_bitcoin_wallet( const fc::path& dir, const std::string& passphrase );

          /** Given a set of user-provided transactions, this method will generate a block that
           * uses transactions prioritized by fee up until the maximum size.  Invalid transactions
           * are ignored and not included in the set.
           *
           * @note some transaction may be valid stand-alone, but may conflict with other transactions.
           */
           trx_block                               generate_next_block( chain_database& db, const signed_transactions& trxs);

           address                                 import_key( const fc::ecc::private_key& key, const std::string& label = "" );
           address                                 new_receive_address( const std::string& label = "" );
           fc::ecc::public_key                     new_public_key( const std::string& label = "" );

           std::unordered_map<address,std::string> get_receive_addresses()const;
           std::string                             get_send_address_label( const address& addr )const;

           bool                                    is_my_address( const address& a )const;
           bool                                    is_my_address( const pts_address& a )const;

           void                                    add_send_address( const address&, const std::string& label = "" );
           std::unordered_map<address,std::string> get_send_addresses()const;

           asset                                   get_balance( asset_type t );
           void                                    set_fee_rate( uint64_t milli_shares_per_byte );

           /** @return milli-shares per byte */
           uint64_t                                get_fee_rate();
           uint64_t                                last_scanned()const;

           output_reference                        get_ref_from_output_idx(output_index idx);

           /** provides the password required to gain access to the private keys
            *  associated with this wallet.
            */
           void                  unlock_wallet( const std::string& key_password, const fc::microseconds& duration = fc::microseconds::maximum());
           /**
            *  removes private keys from memory
            */
           void                  lock_wallet();
           bool                  is_locked()const;

           signed_transaction    transfer( const asset& amnt, const address& to, const std::string& memo = "change" );

           /** returns all transactions issued */
           std::unordered_map<transaction_id_type, transaction_state> get_transaction_history()const;

           void sign_transaction( signed_transaction& trx, const address& addr );
           void sign_transaction( signed_transaction& trx, const std::unordered_set<address>& addresses, bool mark_output_as_used = true);

           bool scan_chain( bts::blockchain::chain_database& chain, uint32_t from_block_num = 0,  scan_progress_callback cb = scan_progress_callback() );
           void mark_as_spent( const output_reference& r );

           void dump_txs(bts::blockchain::chain_database& db, uint32_t count);
           void dump_unspent_outputs();

           const std::map<output_index,trx_output>&  get_unspent_outputs()const;

           std::vector<trx_input> collect_inputs( const asset& min_amnt, asset& total_in, std::unordered_set<address>& req_sigs );

           signed_transaction collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                      std::unordered_set<address>& req_sigs, const address& change_addr);
           signed_transaction collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                      std::unordered_set<address>& req_sigs, const std::string& memo);
           signed_transaction collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                      std::unordered_set<address>& req_sigs);
           signed_transaction collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt,
                                                      const std::string& memo);
           signed_transaction collect_inputs_and_sign(signed_transaction& trx, const asset& min_amnt);

           std::string                         get_transaction_info_string(bts::blockchain::chain_database& db, const transaction& tx);
           virtual std::string                 get_output_info_string(const trx_output& out);
           virtual std::string                 get_input_info_string(bts::blockchain::chain_database& db, const trx_input& in);


        protected:
           virtual void dump_output( const trx_output& out );
           virtual bool scan_output( transaction_state& state, const trx_output& out, const output_reference& ref, const output_index& idx );
           virtual void scan_input( transaction_state& state, const output_reference& ref, const output_index& idx );
           virtual void cache_output( int32_t vote, const trx_output& out, const output_reference& ref, const output_index& idx );

        private:
           bool scan_transaction( transaction_state& trx, uint32_t block_idx, uint32_t trx_idx );
           std::unique_ptr<detail::wallet_impl> my;
   };

   typedef std::shared_ptr<wallet> wallet_ptr;
} } // bts::wallet

FC_REFLECT( bts::wallet::transaction_state, (trx)(memo)(block_num)(to)(from)(delta_balance)(valid) )
FC_REFLECT( bts::wallet::output_index, (block_idx)(trx_idx)(output_idx) )

