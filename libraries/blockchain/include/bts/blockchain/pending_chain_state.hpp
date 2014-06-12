#pragma  once
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

   class pending_chain_state : public chain_interface, public std::enable_shared_from_this<pending_chain_state>
   {
      public:
         pending_chain_state( chain_interface_ptr prev_state = chain_interface_ptr() );

         void set_prev_state( chain_interface_ptr prev_state );

         virtual ~pending_chain_state()override;

         fc::ripemd160                      get_current_random_seed()const override;

         virtual fc::time_point_sec         now()const override;
         virtual int64_t                    get_fee_rate()const override;
         virtual int64_t                    get_delegate_pay_rate()const override;

         virtual oasset_record              get_asset_record( asset_id_type id )const override;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const override;
         virtual oaccount_record            get_account_record( account_id_type id )const override;
         virtual oaccount_record            get_account_record( const address& owner )const override;

         virtual otransaction_record        get_transaction( const transaction_id_type& trx_id, 
                                                             bool exact = true )const override;

         virtual void                       store_transaction( const transaction_id_type&, 
                                                                const transaction_record&  ) override;

         virtual oasset_record              get_asset_record( const string& symbol )const override;
         virtual oaccount_record            get_account_record( const string& name )const override;

         virtual oorder_record              get_bid_record( const market_index_key& )const override;
         virtual oorder_record              get_ask_record( const market_index_key& )const override;
         virtual oorder_record              get_short_record( const market_index_key& )const override;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const override;
                                                                                                            
         virtual void                       store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                       store_collateral_record( const market_index_key& key, const collateral_record& ) override;

         virtual void                       store_proposal_record( const proposal_record& r )override;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const override;
                                                                                                          
         virtual void                       store_proposal_vote( const proposal_vote& r )override;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const override;

         virtual void                       store_asset_record( const asset_record& r )override;
         virtual void                       store_balance_record( const balance_record& r )override;
         virtual void                       store_account_record( const account_record& r )override;

         virtual variant                get_property( chain_property_enum property_id )const override;
         virtual void                       set_property( chain_property_enum property_id, 
                                                          const variant& property_value )override;
         /**
          *  Based upon the current state of the database, calculate any updates that
          *  should be executed in a deterministic manner.
          */
         virtual void                       apply_deterministic_updates()override;

         /** polymorphically allcoate a new state */
         virtual chain_interface_ptr        create( const chain_interface_ptr& prev_state )const;
         /** apply changes from this pending state to the previous state */
         virtual void                       apply_changes()const;

         /** populate undo state with everything that would be necessary to revert this
          * pending state to the previous state.
          */
         virtual void                       get_undo_state( const chain_interface_ptr& undo_state )const;

         /** load the state from a variant */
         virtual void                       from_variant( const variant& v );
         /** convert the state to a variant */
         virtual variant                to_variant()const;


         unordered_map< asset_id_type,         asset_record>            assets;
         unordered_map< account_id_type,       account_record>          accounts;
         unordered_map< balance_id_type,       balance_record>          balances;
         unordered_map< string,           account_id_type>              account_id_index;
         unordered_map< string,           asset_id_type>                symbol_id_index;
         unordered_map< transaction_id_type,   transaction_record>      transactions;
         unordered_map< chain_property_type,   variant>                 properties; 
         unordered_map<proposal_id_type, proposal_record>               proposals;
         unordered_map<address, account_id_type>                        key_to_account;
         map< proposal_vote_id_type, proposal_vote>                     proposal_votes; 
         map< market_index_key, order_record>                           bids; 
         map< market_index_key, order_record>                           asks; 
         map< market_index_key, order_record>                           shorts; 
         map< market_index_key, collateral_record>                      collateral; 

         chain_interface_weak_ptr                                       _prev_state;
   };

   typedef std::shared_ptr<pending_chain_state> pending_chain_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::pending_chain_state,
            (assets)(accounts)(balances)(account_id_index)(symbol_id_index)(transactions)
            (properties)(proposals)(proposal_votes)(bids)(asks)(shorts)(collateral) )
