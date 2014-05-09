#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/block.hpp>

#include <fc/filesystem.hpp>

namespace bts { namespace blockchain {


   namespace detail { class chain_database_impl; }

   struct block_summary
   {
      full_block                                    block_data;
      pending_chain_state_ptr                       applied_changes;
      std::vector<transaction_evaluation_state_ptr> transaction_states;
   };

   class chain_database : public chain_interface, public std::enable_shared_from_this<chain_database>
   {
      public:
         chain_database();
         ~chain_database();

         void open( const fc::path& data_dir );
         void close();

         /** used until DPOS is activated */
         void set_trustee( const fc::ecc::public_key& trustee_key );

         transaction_evaluation_state_ptr              store_pending_transaction( const signed_transaction& trx );
         std::vector<transaction_evaluation_state_ptr> get_pending_transactions()const;

         /** produce a block for the given timeslot, the block is not signed because that is the 
          * role of the wallet.  
          */
         full_block generate_block( fc::time_point_sec timestamp );

         fc::ecc::public_key   get_signing_delegate_key( fc::time_point_sec )const;
         name_id_type          get_signing_delegate_id( fc::time_point_sec )const;
         signed_block_header   get_block_header( const block_id_type& )const;
         signed_block_header   get_block_header( uint32_t block_num )const;
         full_block            get_block( const block_id_type& )const;
         full_block            get_block( uint32_t block_num )const;
         signed_block_header   get_head_block()const;
         osigned_transaction   get_transaction( const transaction_id_type& trx_id )const;
         otransaction_location get_transaction_location( const transaction_id_type& trx_id )const;

         /** should perform any chain reorganization required 
          *  
          *  @return the pending chain state generated as a result of pushing the block,
          *  this state can be used by wallets to scan for changes without the wallets
          *  having to process raw transactions.
          **/
         virtual void  push_block( const full_block& block_data );

         /**
          *  Evaluate the transaction and return the results.
          */
         virtual transaction_evaluation_state_ptr evaluate_transaction( const signed_transaction& trx );


         /** return the timestamp from the head block */
         virtual fc::time_point_sec   timestamp()const;

         /** return the current fee rate in millishares */
         virtual int64_t              get_fee_rate()const;
         virtual int64_t              get_delegate_pay_rate()const;
         std::vector<name_id_type>    get_delegates_by_vote()const;

         virtual void                 remove_asset_record( asset_id_type id )const;
         virtual void                 remove_account_record( const account_id_type& id )const;
         virtual void                 remove_name_record( name_id_type id )const;

         void    scan_assets( const std::function<void( const asset_record& )>& callback );
         void    scan_accounts( const std::function<void( const account_record& )>& callback );
         void    scan_names( const std::function<void( const name_record& )>& callback );

         virtual oasset_record        get_asset_record( asset_id_type id )const;
         virtual oaccount_record      get_account_record( const account_id_type& id )const;
         virtual oname_record         get_name_record( name_id_type id )const;
                                                                                                 
         virtual oasset_record        get_asset_record( const std::string& symbol )const;
         virtual oname_record         get_name_record( const std::string& name )const;
                                                                                                 
         virtual void                 store_asset_record( const asset_record& r );
         virtual void                 store_account_record( const account_record& r );
         virtual void                 store_name_record( const name_record& r );
         virtual void                 store_transaction_location( const transaction_id_type&,  
                                                                  const transaction_location& loc );

         virtual asset_id_type        last_asset_id()const;
         virtual asset_id_type        new_asset_id();
                                      
         virtual name_id_type         last_name_id()const;
         virtual name_id_type         new_name_id();

      private:
         std::unique_ptr<detail::chain_database_impl> my;
   };

   typedef std::shared_ptr<chain_database> chain_database_ptr;

} } // bts::blockchain

