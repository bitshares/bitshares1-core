#pragma once

#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

   namespace detail { class chain_database_impl; }

   class transaction_evaluation_state;
   typedef std::shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

   struct block_summary
   {
      full_block                                    block_data;
      pending_chain_state_ptr                       applied_changes;
   };

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

      /** if at any time this block was determiend to be valid or invalid then this
       * flag will be set.
       */
      fc::optional<bool>           is_valid;
      fc::optional<fc::exception>  invalid_reason;
      bool                         is_included; ///< is included in the current chain database
      bool                         is_known; ///< do we know the content of this block
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
         virtual ~chain_observer(){}

         /**
          * This method is called anytime the blockchain state changes including
          * undo operations.
          */
         virtual void state_changed( const pending_chain_state_ptr& state ) = 0;
         /**
          *  This method is called anytime a block is applied to the chain.
          */
         virtual void block_applied( const block_summary& summary ) = 0;
         virtual void on_pending_transaction( const transaction_evaluation_state_ptr& ) {}
   };

   class chain_database : public chain_interface, public std::enable_shared_from_this<chain_database>
   {
      public:
         chain_database();
         virtual ~chain_database()override;

         /**
          * @brief open Open the databases, reindexing as necessary
          * @param reindex_status_callback Called for each reindexed block, with the count of blocks reindexed so far
          */
         void open(const fc::path& data_dir, fc::optional<fc::path> genesis_file ,
                   std::function<void(uint32_t)> reindex_status_callback = std::function<void(uint32_t)>());
         void close();

         void add_observer( chain_observer* observer );
         void remove_observer( chain_observer* observer );

         void set_relay_fee( share_type shares );
         share_type get_relay_fee();

         void sanity_check()const;

         time_point_sec get_genesis_timestamp()const;

         double get_average_delegate_participation()const;

         /**
          *  When evaluating blocks, you only need to validate the delegate signature assuming you
          *  trust the other delegates to check up on eachother (which everyone should).  This
          *  should greatly reduce processing time for non-delegate nodes.
          */
         void skip_signature_verification( bool state );

         /**
          * The state of the blockchain after applying all pending transactions.
          */
         pending_chain_state_ptr                  get_pending_state()const;

         /**
          *  @param override_limits - stores the transaction even if the pending queue is full, 
          *                           if false then it will require exponential fee increases
          *                           as the queue fills. 
          */
         transaction_evaluation_state_ptr         store_pending_transaction( const signed_transaction& trx, 
                                                                             bool override_limits = true );

         vector<transaction_evaluation_state_ptr> get_pending_transactions()const;
         bool                                     is_known_transaction( const transaction_id_type& trx_id );

         /** Produce a block for the given timeslot, the block is not signed because that is the
          *  role of the wallet.
          */
         full_block                  generate_block( const time_point_sec& timestamp );

         /**
          *  The chain ID is the hash of the initial_config loaded when the
          *  database was first created.
          */
         digest_type                 chain_id()const;

         optional<block_fork_data>   get_block_fork_data( const block_id_type& )const; //is_known_block( const block_id_type& block_id )const;
         bool                        is_known_block( const block_id_type& id )const;
         bool                        is_included_block( const block_id_type& id )const;
         //optional<block_fork_data> is_included_block( const block_id_type& block_id )const;

         fc::ripemd160               get_current_random_seed()const override;

         account_record              get_delegate_record_for_signee( const public_key_type& block_signee )const;
         account_record              get_block_signee( const block_id_type& block_id )const;
         account_record              get_block_signee( uint32_t block_num )const;

         account_record              get_slot_signee( const time_point_sec& timestamp,
                                                      const std::vector<account_id_type>& ordered_delegates )const;

         optional<time_point_sec>    get_next_producible_block_timestamp( const vector<account_id_type>& delegate_ids )const;

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


         virtual oprice              get_median_delegate_price( asset_id_type )const override;
         vector<feed_record>         get_feeds_for_asset( asset_id_type asset_id )const;
         vector<feed_record>         get_feeds_from_delegate( account_id_type delegate_id )const;

         virtual odelegate_slate      get_delegate_slate( slate_id_type id )const override;
         virtual void                 store_delegate_slate( slate_id_type id, 
                                                            const delegate_slate& slate ) override;

         virtual otransaction_record  get_transaction( const transaction_id_type& trx_id, 
                                                       bool exact = true )const override;

         virtual void             store_transaction( const transaction_id_type&, 
                                                     const transaction_record&  ) override;

         vector<account_record >  get_accounts( const string& first, 
                                                uint32_t count )const;

         vector<asset_record>     get_assets( const string& first_symbol, 
                                              uint32_t count )const;

         std::vector<slot_record> get_delegate_slot_records( const account_id_type& delegate_id )const;

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
         virtual transaction_evaluation_state_ptr evaluate_transaction( const signed_transaction& trx, const share_type& required_fees = 0 );
         optional<fc::exception> get_transaction_error( const signed_transaction& transaction, const share_type& min_fee );

         /** return the timestamp from the head block */
         virtual time_point_sec         now()const override;

         /** top delegates by current vote, projected to be active in the next round */
         vector<account_id_type>            next_round_active_delegates()const;
                                            
         vector<account_id_type>            get_delegates_by_vote( uint32_t first=0, uint32_t count = uint32_t(-1) )const;
         vector<account_record>             get_delegate_records_by_vote( uint32_t first=0, uint32_t count = uint32_t(-1))const;
#if 0
         vector<proposal_record>            get_proposals( uint32_t first=0, uint32_t count = uint32_t(-1))const;
         vector<proposal_vote>              get_proposal_votes( proposal_id_type proposal_id ) const;
#endif

         fc::variant_object                 find_delegate_vote_discrepancies() const;

         optional<market_order>             get_market_bid( const market_index_key& )const;
         vector<market_order>               get_market_bids( const string& quote_symbol, 
                                                             const string& base_symbol, 
                                                             uint32_t limit = uint32_t(-1) );

         optional<market_order>             get_market_short( const market_index_key& )const;
         vector<market_order>               get_market_shorts( const string& quote_symbol, 
                                                               uint32_t limit = uint32_t(-1) );
         vector<market_order>               get_market_covers( const string& quote_symbol, 
                                                               uint32_t limit = uint32_t(-1) );

         virtual omarket_order              get_lowest_ask_record( asset_id_type quote_id, asset_id_type base_id ) override; 
         optional<market_order>             get_market_ask( const market_index_key& )const;
         vector<market_order>               get_market_asks( const string& quote_symbol, 
                                                             const string& base_symbol, 
                                                             uint32_t limit = uint32_t(-1) );

         void                               scan_assets( function<void( const asset_record& )> callback );
         void                               scan_balances( function<void( const balance_record& )> callback );
         void                               scan_accounts( function<void( const account_record& )> callback );

         virtual variant                    get_property( chain_property_enum property_id )const override;
         virtual void                       set_property( chain_property_enum property_id, 
                                                          const variant& property_value )override;

         bool                               is_valid_symbol( const string& asset_symbol )const;
         string                             get_asset_symbol( asset_id_type asset_id )const;
         asset_id_type                      get_asset_id( const string& asset_symbol )const;
         virtual oasset_record              get_asset_record( asset_id_type id )const override;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const override;
         virtual oaccount_record            get_account_record( account_id_type id )const override;
         virtual oaccount_record            get_account_record( const address& owner )const override;

         virtual oasset_record              get_asset_record( const string& symbol )const override;
         virtual oaccount_record            get_account_record( const string& name )const override;

         virtual void                       store_asset_record( const asset_record& r )override;
         virtual void                       store_balance_record( const balance_record& r )override;
         virtual void                       store_account_record( const account_record& r )override;

         virtual vector<operation>          get_recent_operations( operation_type_enum t )override;
         virtual void                       store_recent_operation( const operation& o )override;

#if 0
         virtual void                       store_proposal_record( const proposal_record& r )override;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const override;
                                                                                                          
         virtual void                       store_proposal_vote( const proposal_vote& r )override;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const override;
#endif

         virtual oorder_record              get_bid_record( const market_index_key& )const override;
         virtual oorder_record              get_ask_record( const market_index_key& )const override;
         virtual oorder_record              get_short_record( const market_index_key& )const override;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const override;
                                                                                                            
         virtual void                       store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_collateral_record( const market_index_key& key, const collateral_record& ) override;

         virtual void                       store_slot_record( const slot_record& r )override;
         virtual oslot_record               get_slot_record( const time_point_sec& start_time )const override;

         virtual omarket_status             get_market_status( asset_id_type quote_id, asset_id_type base_id ) override;
         virtual void                       store_market_status( const market_status& s ) override;
         virtual void                       store_market_history_record( const market_history_key &key, const market_history_record &record ) override;
         virtual omarket_history_record     get_market_history_record( const market_history_key &key ) const override;
         market_history_points              get_market_price_history(asset_id_type quote_id,
                                                                      asset_id_type base_id,
                                                                      const fc::time_point& start_time,
                                                                      const fc::microseconds& duration,
                                                                      market_history_key::time_granularity_enum granularity );

         virtual void                       set_market_transactions( vector<market_transaction> trxs );
         vector<market_transaction>         get_market_transactions( uint32_t block_num  )const;

         virtual void                       set_feed( const feed_record&  ) override;
         virtual ofeed_record               get_feed( const feed_index& )const override;

         asset                              unclaimed_genesis();

         // TODO... only call on pending chain state
         virtual void                       set_market_dirty( asset_id_type quote_id, asset_id_type base_id ) override { FC_ASSERT( false, "this shouldn't be called directly" ); }
      private:
         unique_ptr<detail::chain_database_impl> my;
   };

   typedef shared_ptr<chain_database> chain_database_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::block_fork_data, (next_blocks)(is_linked)(is_valid)(invalid_reason)(is_included)(is_known) )
FC_REFLECT( bts::blockchain::fork_record, (block_id)(signing_delegate)(transaction_count)(latency)(size)(timestamp)(is_valid)(invalid_reason)(is_current_fork) )
