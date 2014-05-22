#pragma  once
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

   class pending_chain_state : public chain_interface
   {
      public:
         pending_chain_state( chain_interface_ptr prev_state = chain_interface_ptr() );

         void set_prev_state( chain_interface_ptr prev_state );

         virtual ~pending_chain_state()override;

         virtual std::vector<name_id_type>  get_active_delegates()const override;

         digest_type                        get_current_random_seed()const override;

         virtual fc::time_point_sec         now()const override;
         virtual int64_t                    get_fee_rate()const override;
         virtual int64_t                    get_delegate_pay_rate()const override;

         virtual oasset_record              get_asset_record( asset_id_type id )const override;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const override;
         virtual oname_record               get_name_record( name_id_type id )const override;
         virtual otransaction_location      get_transaction_location( const transaction_id_type& )const override;

         virtual oasset_record              get_asset_record( const std::string& symbol )const override;
         virtual oname_record               get_name_record( const std::string& name )const override;

         virtual void                       store_proposal_record( const proposal_record& r )override;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const override;
                                                                                                          
         virtual void                       store_proposal_vote( const proposal_vote& r )override;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const override;

         virtual void                       store_asset_record( const asset_record& r )override;
         virtual void                       store_balance_record( const balance_record& r )override;
         virtual void                       store_name_record( const name_record& r )override;
         virtual void                       store_transaction_location( const transaction_id_type&,
                                                                        const transaction_location& loc )override;

         virtual fc::variant                get_property( chain_property_enum property_id )const override;
         virtual void                       set_property( chain_property_enum property_id, 
                                                          const fc::variant& property_value )override;
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
         virtual void                       from_variant( const fc::variant& v );
         /** convert the state to a variant */
         virtual fc::variant                to_variant()const;


         std::unordered_map< asset_id_type,         asset_record>         assets;
         std::unordered_map< name_id_type,          name_record>          names;
         std::unordered_map< balance_id_type,       balance_record>       balances;
         std::unordered_map< std::string,           name_id_type>         name_id_index;
         std::unordered_map< std::string,           asset_id_type>        symbol_id_index;
         std::unordered_map< transaction_id_type,   transaction_location> unique_transactions;
         std::unordered_map< chain_property_type,   fc::variant>          properties; 
         std::unordered_map<proposal_id_type, proposal_record>            proposals;
         std::map< proposal_vote_id_type, proposal_vote>                  proposal_votes; 

         chain_interface_ptr                                            _prev_state;
   };

   typedef std::shared_ptr<pending_chain_state> pending_chain_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::pending_chain_state,
            (assets)(names)(balances)(name_id_index)(symbol_id_index)(unique_transactions)
            (properties)(proposals)(proposal_votes) )
