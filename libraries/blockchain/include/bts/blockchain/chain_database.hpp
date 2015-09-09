#pragma once

#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/delegate_config.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

   namespace detail { class chain_database_impl; }

   struct transaction_evaluation_state;
   typedef std::shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

   struct block_fork_data
   {
      block_fork_data():is_linked(false),is_included(false),is_known(false){}

      bool invalid()const
      {
         if( !!is_valid ) return !*is_valid;
         return false;
      }
      bool valid()const
      {
         if( !!is_valid ) return *is_valid;
         return false;
      }
      bool can_link()const
      {
         return is_linked && !invalid();
      }

      std::unordered_set<block_id_type> next_blocks; ///< IDs of all blocks that come after
      bool                              is_linked;   ///< is linked to genesis block

      /** if at any time this block was determined to be valid or invalid then this
       * flag will be set.
       */
      fc::optional<bool>           is_valid;
      fc::optional<fc::exception>  invalid_reason;
      bool                         is_included; ///< is included in the current chain database
      bool                         is_known; ///< do we know the content of this block (false if placeholder)
   };

   struct fork_record
   {
       fork_record()
        : signing_delegate(0),
          transaction_count(0),
          latency(0),
          size(0),
          is_current_fork(false)
       {}

       block_id_type block_id;
       account_id_type signing_delegate;
       uint32_t transaction_count;
       fc::microseconds latency;
       uint32_t size;
       fc::time_point_sec timestamp;
       fc::optional<bool> is_valid;
       fc::optional<fc::exception> invalid_reason;
       bool is_current_fork;
   };
   typedef fc::optional<fork_record> ofork_record;

   class chain_observer
   {
      public:
         virtual ~chain_observer() {}
         virtual void block_pushed( const full_block& ) = 0;
         virtual void block_popped( const pending_chain_state_ptr& ) = 0;
   };

   class chain_database : public chain_interface, public std::enable_shared_from_this<chain_database>
   {
      public:
         chain_database();
         virtual ~chain_database()override;

         void open( const fc::path& data_dir, const fc::optional<fc::path>& genesis_file, const bool statistics_enabled,
                    const std::function<void(float)> replay_status_callback = std::function<void(float)>() );
         void close();

         void add_observer( chain_observer* observer );
         void remove_observer( chain_observer* observer );

         void set_relay_fee( share_type shares );
         share_type get_relay_fee();

         double get_average_delegate_participation()const;

         /**
          * The state of the blockchain after applying all pending transactions.
          */
         pending_chain_state_ptr                    get_pending_state()const;

         /**
          *  @param override_limits - stores the transaction even if the pending queue is full,
          *                           if false then it will require exponential fee increases
          *                           as the queue fills.
          */
         transaction_evaluation_state_ptr           store_pending_transaction( const signed_transaction& trx,
                                                                             bool override_limits = true );

         vector<transaction_evaluation_state_ptr>   get_pending_transactions()const;
         virtual bool                               is_known_transaction( const transaction& trx )const override;

         /** Produce a block for the given timeslot, the block is not signed because that is the
          *  role of the wallet.
          */
         full_block                  generate_block( const time_point_sec block_timestamp,
                                                     const delegate_config& config = delegate_config() );

         optional<block_fork_data>   get_block_fork_data( const block_id_type& )const;
         bool                        is_known_block( const block_id_type& id )const;
         bool                        is_included_block( const block_id_type& id )const;

         account_record              get_delegate_record_for_signee( const public_key_type& block_signee )const;
         account_record              get_block_signee( const block_id_type& block_id )const;
         account_record              get_block_signee( uint32_t block_num )const;

         account_record              get_slot_signee( const time_point_sec timestamp,
                                                      const std::vector<account_id_type>& ordered_delegates )const;

         optional<time_point_sec>    get_next_producible_block_timestamp( const vector<account_id_type>& delegate_ids )const;

         vector<transaction_record>  fetch_address_transactions( const address& addr );

         uint32_t                    find_block_num(fc::time_point_sec &time)const;
         uint32_t                    get_block_num( const block_id_type& )const;
         signed_block_header         get_block_header( const block_id_type& )const;
         signed_block_header         get_block_header( uint32_t block_num )const;
         digest_block                get_block_digest( const block_id_type& )const;
         digest_block                get_block_digest( uint32_t block_num )const;
         full_block                  get_block( const block_id_type& )const;
         full_block                  get_block( uint32_t block_num )const;
         vector<transaction_record>  get_transactions_for_block( const block_id_type& )const;
         signed_block_header         get_head_block()const;
         virtual uint32_t            get_head_block_num()const override;
         block_id_type               get_head_block_id()const;
         block_id_type               get_block_id( uint32_t block_num )const;
         oblock_record               get_block_record( const block_id_type& block_id )const;
         oblock_record               get_block_record( uint32_t block_num )const;

         virtual oprice              get_active_feed_price( const asset_id_type quote_id )const override;

         vector<feed_record>         get_feeds_for_asset( const asset_id_type quote_id, const asset_id_type base_id )const;
         vector<feed_record>         get_feeds_from_delegate( const account_id_type delegate_id )const;

         virtual otransaction_record get_transaction( const transaction_id_type& trx_id,
                                                      bool exact = true )const override;

         virtual void                store_transaction( const transaction_id_type&,
                                                        const transaction_record&  ) override;

         vector<burn_record>         fetch_burn_records( const string& account_name )const;

         unordered_map<balance_id_type, balance_record>     get_balances_for_address( const address& addr )const;
         unordered_map<balance_id_type, balance_record>     get_balances_for_key( const public_key_type& key )const;
         vector<account_record>                             get_accounts( const string& first,
                                                                          uint32_t limit )const;

         vector<asset_record>                               get_assets( const string& first_symbol,
                                                                        uint32_t limit )const;

         std::vector<slot_record> get_delegate_slot_records( const account_id_type delegate_id, uint32_t limit )const;

         std::map<uint32_t, std::vector<fork_record> > get_forks_list()const;
         std::string export_fork_graph( uint32_t start_block = 1, uint32_t end_block = -1, const fc::path& filename = "" )const;

         /** should perform any chain reorganization required
          *
          *  @return the pending chain state generated as a result of pushing the block,
          *  this state can be used by wallets to scan for changes without the wallets
          *  having to process raw transactions.
          **/
         block_fork_data push_block(const full_block& block_data);

         vector<block_id_type> get_fork_history( const block_id_type& id );

         /**
          *  Evaluate the transaction and return the results.
          */
         virtual transaction_evaluation_state_ptr   evaluate_transaction( const signed_transaction& trx, const share_type required_fees = 0 );
         optional<fc::exception>                    get_transaction_error( const signed_transaction& transaction, const share_type min_fee );

         /** return the timestamp from the head block */
         virtual time_point_sec             now()const override;

         /** top delegates by current vote, projected to be active in the next round */
         vector<account_id_type>            next_round_active_delegates()const;
         vector<account_id_type>            get_delegates_by_vote( uint32_t first=0, uint32_t count = uint32_t(-1) )const;

         optional<market_order>             get_market_bid( const market_index_key& )const;
         vector<market_order>               get_market_bids( const string& quote_symbol,
                                                             const string& base_symbol,
                                                             uint32_t limit = uint32_t(-1) )const;

         optional<market_order>             get_market_short( const market_index_key& )const;
         vector<market_order>               get_market_shorts( const string& quote_symbol,
                                                               uint32_t limit = uint32_t(-1) )const;
         vector<market_order>               get_market_covers( const string& quote_symbol,
                                                               const string& base_symbol = BTS_BLOCKCHAIN_SYMBOL,
                                                               uint32_t limit = uint32_t(-1) )const;

         share_type                         get_asset_collateral( const string& symbol );

         virtual omarket_order              get_lowest_ask_record( const asset_id_type quote_id,
                                                                   const asset_id_type base_id )override;
         optional<market_order>             get_market_ask( const market_index_key& )const;
         vector<market_order>               get_market_asks( const string& quote_symbol,
                                                             const string& base_symbol,
                                                             uint32_t limit = uint32_t(-1) )const;

         optional<market_order>             get_market_order( const order_id_type& order_id, order_type_enum type = null_order )const;

         vector<market_order>               scan_market_orders( std::function<bool( const market_order& )> filter,
                                                                uint32_t limit = -1, order_type_enum type = null_order )const;

         void                               scan_unordered_accounts( const function<void( const account_record& )> )const;
         void                               scan_ordered_accounts( const function<void( const account_record& )> )const;
         void                               scan_unordered_assets( const function<void( const asset_record& )> )const;
         void                               scan_ordered_assets( const function<void( const asset_record& )> )const;
         void                               scan_balances( const function<void( const balance_record& )> callback )const;
         void                               scan_transactions( const function<void( const transaction_record& )> callback )const;

         bool                               is_valid_symbol( const string& asset_symbol )const;
         string                             get_asset_symbol( const asset_id_type asset_id )const;
         asset_id_type                      get_asset_id( const string& asset_symbol )const;

         virtual vector<operation>          get_recent_operations( operation_type_enum t )const;
         virtual void                       store_recent_operation( const operation& o );

         virtual oorder_record              get_bid_record( const market_index_key& )const override;
         virtual oorder_record              get_ask_record( const market_index_key& )const override;
         virtual oorder_record              get_short_record( const market_index_key& )const override;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const override;

         virtual void                       store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_collateral_record( const market_index_key& key, const collateral_record& ) override;

         virtual void                       store_market_history_record( const market_history_key &key, const market_history_record &record ) override;
         virtual omarket_history_record     get_market_history_record( const market_history_key &key )const override;
         market_history_points              get_market_price_history( const asset_id_type quote_id,
                                                                      const asset_id_type base_id,
                                                                      const fc::time_point start_time,
                                                                      const fc::microseconds duration,
                                                                      market_history_key::time_granularity_enum granularity )const;

         virtual void                       set_market_transactions( vector<market_transaction> trxs )override;
         vector<market_transaction>         get_market_transactions( uint32_t block_num  )const;

         vector<pair<asset_id_type, asset_id_type>> get_market_pairs()const;

         vector<order_history_record>       market_order_history(asset_id_type quote,
                                                                  asset_id_type base,
                                                                  uint32_t skip_count,
                                                                  uint32_t limit,
                                                                  const address& owner );

         void                               generate_snapshot( const fc::path& filename )const;
         void                               graphene_snapshot( const string& filename, const set<string>& whitelist )const;
         void                               generate_issuance_map( const string& symbol, const fc::path& filename )const;

         unordered_map<asset_id_type, share_type> calculate_supplies()const;
         unordered_map<asset_id_type, share_type> calculate_debts( bool include_interest )const;
         share_type calculate_max_core_supply( share_type pay_per_block )const;

         asset                              unclaimed_genesis();

         // XXX: Only call on pending chain state
         virtual void                       set_market_dirty( const asset_id_type quote_id, const asset_id_type base_id )override
         {
             FC_ASSERT( false, "this shouldn't be called directly" );
         }

         fc::variants debug_get_matching_errors() const;
         void debug_trap_on_block( uint32_t blocknum );

         // Applies only when pushing new blocks; gets enabled in delegate loop
         bool _verify_transaction_signatures = false;

         bool _debug_verify_market_matching = false;

      private:
         unique_ptr<detail::chain_database_impl> my;

         virtual oproperty_record property_lookup_by_id( const property_id_type )const override;
         virtual void property_insert_into_id_map( const property_id_type, const property_record& )override;
         virtual void property_erase_from_id_map( const property_id_type )override;

         virtual oaccount_record account_lookup_by_id( const account_id_type )const override;
         virtual oaccount_record account_lookup_by_name( const string& )const override;
         virtual oaccount_record account_lookup_by_address( const address& )const override;

         virtual void account_insert_into_id_map( const account_id_type, const account_record& )override;
         virtual void account_insert_into_name_map( const string&, const account_id_type )override;
         virtual void account_insert_into_address_map( const address&, const account_id_type )override;
         virtual void account_insert_into_vote_set( const vote_del& )override;

         virtual void account_erase_from_id_map( const account_id_type )override;
         virtual void account_erase_from_name_map( const string& )override;
         virtual void account_erase_from_address_map( const address& )override;
         virtual void account_erase_from_vote_set( const vote_del& )override;

         virtual oasset_record asset_lookup_by_id( const asset_id_type )const override;
         virtual oasset_record asset_lookup_by_symbol( const string& )const override;

         virtual void asset_insert_into_id_map( const asset_id_type, const asset_record& )override;
         virtual void asset_insert_into_symbol_map( const string&, const asset_id_type )override;

         virtual void asset_erase_from_id_map( const asset_id_type )override;
         virtual void asset_erase_from_symbol_map( const string& )override;

         virtual oslate_record slate_lookup_by_id( const slate_id_type )const override;
         virtual void slate_insert_into_id_map( const slate_id_type, const slate_record& )override;
         virtual void slate_erase_from_id_map( const slate_id_type )override;

         virtual obalance_record balance_lookup_by_id( const balance_id_type& )const override;
         virtual void balance_insert_into_id_map( const balance_id_type&, const balance_record& )override;
         virtual void balance_erase_from_id_map( const balance_id_type& )override;

         virtual otransaction_record transaction_lookup_by_id( const transaction_id_type& )const override;

         virtual void transaction_insert_into_id_map( const transaction_id_type&, const transaction_record& )override;
         virtual void transaction_insert_into_unique_set( const transaction& )override;

         virtual void transaction_erase_from_id_map( const transaction_id_type& )override;
         virtual void transaction_erase_from_unique_set( const transaction& )override;

         virtual oburn_record burn_lookup_by_index( const burn_index& )const override;
         virtual void burn_insert_into_index_map( const burn_index&, const burn_record& )override;
         virtual void burn_erase_from_index_map( const burn_index& )override;

         virtual ostatus_record status_lookup_by_index( const status_index )const override;
         virtual void status_insert_into_index_map( const status_index, const status_record& )override;
         virtual void status_erase_from_index_map( const status_index )override;

         virtual ofeed_record feed_lookup_by_index( const feed_index )const override;
         virtual void feed_insert_into_index_map( const feed_index, const feed_record& )override;
         virtual void feed_erase_from_index_map( const feed_index )override;

         virtual oslot_record slot_lookup_by_index( const slot_index )const override;
         virtual oslot_record slot_lookup_by_timestamp( const time_point_sec )const override;

         virtual void slot_insert_into_index_map( const slot_index, const slot_record& )override;
         virtual void slot_insert_into_timestamp_map( const time_point_sec, const account_id_type )override;

         virtual void slot_erase_from_index_map( const slot_index )override;
         virtual void slot_erase_from_timestamp_map( const time_point_sec )override;
   };
   typedef shared_ptr<chain_database> chain_database_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::block_fork_data, (next_blocks)(is_linked)(is_valid)(invalid_reason)(is_included)(is_known) )
FC_REFLECT( bts::blockchain::fork_record, (block_id)(signing_delegate)(transaction_count)(latency)(size)(timestamp)(is_valid)(invalid_reason)(is_current_fork) )
