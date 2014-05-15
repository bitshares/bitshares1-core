#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/block.hpp>

#include <fc/filesystem.hpp>

#include <functional>

namespace bts { namespace blockchain {


   namespace detail { class chain_database_impl; }

   struct block_summary
   {
      full_block                                    block_data;
      pending_chain_state_ptr                       applied_changes;
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
   };

   class chain_database : public chain_interface, public std::enable_shared_from_this<chain_database>
   {
      public:
         chain_database();
         virtual ~chain_database() override;

         void open( const fc::path& data_dir, fc::optional<fc::path> genesis_file = fc::optional<fc::path>() );
         void close();

         void set_observer( chain_observer* observer );

         transaction_evaluation_state_ptr              store_pending_transaction( const signed_transaction& trx );
         std::vector<transaction_evaluation_state_ptr> get_pending_transactions()const;
         bool                                          is_known_transaction( const transaction_id_type& trx_id );

         /** produce a block for the given timeslot, the block is not signed because that is the
          * role of the wallet.
          */
         full_block                    generate_block( fc::time_point_sec timestamp );

         bool                          is_known_block( const block_id_type& block_id )const;

         fc::ecc::public_key           get_signing_delegate_key( fc::time_point_sec )const;
         name_id_type                  get_signing_delegate_id( fc::time_point_sec )const;
         uint32_t                      get_block_num( const block_id_type& )const;
         signed_block_header           get_block_header( const block_id_type& )const;
         signed_block_header           get_block_header( uint32_t block_num )const;
         full_block                    get_block( const block_id_type& )const;
         full_block                    get_block( uint32_t block_num )const;
         signed_block_header           get_head_block()const;
         uint32_t                      get_head_block_num()const;
         block_id_type                 get_head_block_id()const;
         osigned_transaction           get_transaction( const transaction_id_type& trx_id )const;
         virtual otransaction_location get_transaction_location( const transaction_id_type& trx_id )const override;

         std::vector<name_record> get_names( const std::string& first, uint32_t count )const;

         /** should perform any chain reorganization required
          *
          *  @return the pending chain state generated as a result of pushing the block,
          *  this state can be used by wallets to scan for changes without the wallets
          *  having to process raw transactions.
          **/
         virtual void push_block( const full_block& block_data );


         /**
          *  Evaluate the transaction and return the results.
          */
         virtual transaction_evaluation_state_ptr evaluate_transaction( const signed_transaction& trx );


         /** return the timestamp from the head block */
         virtual fc::time_point_sec   timestamp()const override;

         /** return the current fee rate in millishares */
         virtual int64_t              get_fee_rate()const override;
         virtual int64_t              get_delegate_pay_rate()const override;
         std::vector<name_id_type>    get_active_delegates()const;
         std::vector<name_id_type>    get_delegates_by_vote( uint32_t first=0, uint32_t count = -1 )const;
         std::vector<name_record>     get_delegate_records_by_vote( uint32_t first=0, uint32_t count = -1)const;

         virtual void                 remove_asset_record( asset_id_type id )const;
         virtual void                 remove_balance_record( const balance_id_type& id )const;
         virtual void                 remove_name_record( name_id_type id )const;

         void                         scan_assets( const std::function<void( const asset_record& )>& callback );
         void                         scan_balances( const std::function<void( const balance_record& )>& callback );
         void                         scan_names( const std::function<void( const name_record& )>& callback );

         virtual oasset_record        get_asset_record( asset_id_type id )const override;
         virtual obalance_record      get_balance_record( const balance_id_type& id )const override;
         virtual oname_record         get_name_record( name_id_type id )const override;

         virtual oasset_record        get_asset_record( const std::string& symbol )const override;
         virtual oname_record         get_name_record( const std::string& name )const override;

         virtual void                 store_asset_record( const asset_record& r ) override;
         virtual void                 store_balance_record( const balance_record& r ) override;
         virtual void                 store_name_record( const name_record& r ) override;
         virtual void                 store_transaction_location( const transaction_id_type&,
                                                                  const transaction_location& loc ) override;

         virtual asset_id_type        last_asset_id()const override;
         virtual asset_id_type        new_asset_id() override;

         virtual name_id_type         last_name_id()const override;
         virtual name_id_type         new_name_id() override;

      private:
         std::unique_ptr<detail::chain_database_impl> my;
   };

   typedef std::shared_ptr<chain_database> chain_database_ptr;

} } // bts::blockchain

