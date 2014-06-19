#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/block.hpp>

#include <fc/filesystem.hpp>

#include <functional>

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
      fc::optional<bool>         is_valid;
      bool                       is_included; ///< is included in the current chain database
      bool                       is_known; ///< do we know the content of this block
   };

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

         void open( const fc::path& data_dir, fc::optional<fc::path> genesis_file );
         void close();

         void add_observer( chain_observer* observer );
         void remove_observer( chain_observer* observer );

         void sanity_check()const;

         double get_average_delegate_participation()const;

         transaction_evaluation_state_ptr         store_pending_transaction( const signed_transaction& trx );
         vector<transaction_evaluation_state_ptr> get_pending_transactions()const;
         bool                                     is_known_transaction( const transaction_id_type& trx_id );

         /** Produce a block for the given timeslot, the block is not signed because that is the
          *  role of the wallet.
          */
         full_block               generate_block( time_point_sec timestamp );

         /**
          *  The chain ID is the hash of the initial_config loaded when the
          *  database was first created.
          */
         digest_type              chain_id()const;

         optional<block_fork_data> get_block_fork_data( const block_id_type& )const; //is_known_block( const block_id_type& block_id )const;
         bool                      is_known_block( const block_id_type& id )const;
         bool                      is_included_block( const block_id_type& id )const;
         //optional<block_fork_data>                      is_included_block( const block_id_type& block_id )const;

         fc::ripemd160               get_current_random_seed()const override;

         account_id_type             get_signing_delegate_id( const fc::time_point_sec& block_timestamp,
                                                              const std::vector<account_id_type>& sorted_delegates )const;
         public_key_type             get_signing_delegate_key( const fc::time_point_sec& block_timestamp,
                                                               const std::vector<account_id_type>& sorted_delegates )const;

         account_id_type             get_signing_delegate_id( const fc::time_point_sec& block_timestamp )const;
         public_key_type             get_signing_delegate_key( const fc::time_point_sec& block_timestamp )const;

         uint32_t                    get_block_num( const block_id_type& )const;
         signed_block_header         get_block_header( const block_id_type& )const;
         signed_block_header         get_block_header( uint32_t block_num )const;
         digest_block                get_block_digest( const block_id_type& )const;
         digest_block                get_block_digest( uint32_t block_num )const;
         full_block                  get_block( const block_id_type& )const;
         full_block                  get_block( uint32_t block_num )const;
         vector<transaction_record>  get_transactions_for_block( const block_id_type& )const;
         signed_block_header      get_head_block()const;
         uint32_t                 get_head_block_num()const;
         block_id_type            get_head_block_id()const;
         block_id_type            get_block_id( uint32_t block_num )const;
         oblock_record            get_block_record( const block_id_type& block_id )const;
         oblock_record            get_block_record( uint32_t block_num )const;

         virtual otransaction_record  get_transaction( const transaction_id_type& trx_id, 
                                                       bool exact = true )const override;

         virtual void             store_transaction( const transaction_id_type&, 
                                                     const transaction_record&  ) override;

         vector<account_record >  get_accounts( const string& first, 
                                                uint32_t count )const;

         vector<asset_record>     get_assets( const string& first_symbol, 
                                              uint32_t count )const;

         std::map<uint32_t, delegate_block_stats> get_delegate_block_stats( const account_id_type& delegate_id )const;

         std::vector<uint32_t> get_forks_list()const;
         std::string export_fork_graph( uint32_t start_block = 1, uint32_t end_block = -1, const fc::path& filename = "" )const;

         /** should perform any chain reorganization required
          *
          *  @return the pending chain state generated as a result of pushing the block,
          *  this state can be used by wallets to scan for changes without the wallets
          *  having to process raw transactions.
          **/
         block_fork_data push_block( const full_block& block_data );

         vector<block_id_type> get_fork_history( const block_id_type& id );

         /**
          *  Evaluate the transaction and return the results.
          */
         virtual transaction_evaluation_state_ptr evaluate_transaction( const signed_transaction& trx );


         /** return the timestamp from the head block */
         virtual time_point_sec         now()const override;

         /** return the current fee rate in millishares */
         virtual int64_t                    get_fee_rate()const override;
         virtual int64_t                    get_delegate_pay_rate()const override;

         /** top delegates by current vote, projected to be active in the next round */
         vector<account_id_type>            next_round_active_delegates()const;
         vector<account_id_type>            current_round_active_delegates()const;
                                            
         vector<account_id_type>            get_delegates_by_vote( uint32_t first=0, uint32_t count = uint32_t(-1) )const;
         vector<account_record>             get_delegate_records_by_vote( uint32_t first=0, uint32_t count = uint32_t(-1))const;
         vector<proposal_record>            get_proposals( uint32_t first=0, uint32_t count = uint32_t(-1))const;
         vector<proposal_vote>              get_proposal_votes( proposal_id_type proposal_id ) const;

         optional<market_order>             get_market_bid( const market_index_key& )const;
         vector<market_order>               get_market_bids( const string& quote_symbol, 
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
         asset_id_type                      get_asset_id( const string& asset_sybmol )const;
         virtual oasset_record              get_asset_record( asset_id_type id )const override;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const override;
         virtual oaccount_record            get_account_record( account_id_type id )const override;
         virtual oaccount_record            get_account_record( const address& owner )const override;

         virtual oasset_record              get_asset_record( const string& symbol )const override;
         virtual oaccount_record            get_account_record( const string& name )const override;

         virtual void                       store_asset_record( const asset_record& r )override;
         virtual void                       store_balance_record( const balance_record& r )override;
         virtual void                       store_account_record( const account_record& r )override;

         virtual void                       store_proposal_record( const proposal_record& r )override;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const override;
                                                                                                          
         virtual void                       store_proposal_vote( const proposal_vote& r )override;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const override;

         virtual oorder_record              get_bid_record( const market_index_key& )const override;
         virtual oorder_record              get_ask_record( const market_index_key& )const override;
         virtual oorder_record              get_short_record( const market_index_key& )const override;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const override;
                                                                                                            
         virtual void                       store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_collateral_record( const market_index_key& key, const collateral_record& ) override;

      private:
         unique_ptr<detail::chain_database_impl> my;
   };

   typedef shared_ptr<chain_database> chain_database_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::block_fork_data, (next_blocks)(is_linked)(is_valid)(is_included)(is_known) )
