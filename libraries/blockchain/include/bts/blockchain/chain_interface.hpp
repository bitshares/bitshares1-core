#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {
   typedef fc::optional<signed_transaction> osigned_transaction;

   struct delegate_stats
   {
      delegate_stats()
      :votes_for(0),
       votes_against(0),
       blocks_produced(0),
       blocks_missed(0),
       pay_balance(0){}

      share_type                     votes_for;
      share_type                     votes_against;
      uint32_t                       blocks_produced;
      uint32_t                       blocks_missed;
      secret_hash_type               next_secret_hash;
      uint32_t                       last_block_num_produced;

      /**
       *  Delegate pay is held in escrow and may be siezed 
       *  and returned to the shareholders if they are fired
       *  for provable cause.
       */
      share_type                     pay_balance;
   };

   struct proposal_record
   {
      bool is_null()const { return submitting_delegate_id == -1; }
      proposal_record make_null()const    { proposal_record cpy(*this); cpy.submitting_delegate_id = -1; return cpy; }

      proposal_id_type      id;
      name_id_type          submitting_delegate_id; // the delegate_id of the submitter
      fc::time_point_sec    submission_date;
      std::string           subject;
      std::string           body;
      std::string           proposal_type; // alert, bug fix, feature upgrade, property change, etc
      fc::variant           data;  // data that is unique to the proposal
   };
   typedef fc::optional<proposal_record> oproposal_record;
   
   struct proposal_vote
   {
      enum vote_type 
      {
          no  = 0,
          yes = 1,
          undefined = 2
      };
      bool is_null()const { return vote == undefined; }
      proposal_vote make_null()const { proposal_vote cpy(*this); cpy.vote = undefined; return cpy; }

      proposal_vote_id_type             id;
      fc::time_point_sec                timestamp;
      fc::enum_type<uint8_t,vote_type>  vote;
   };
   typedef fc::optional<proposal_vote> oproposal_vote;

   /**
    */
   struct balance_record
   {
      balance_record():balance(0){}
      balance_record( const withdraw_condition& c )
      :balance(0),condition(c){}

      balance_record( const address& owner, const asset& balance, name_id_type delegate_id );

      /** condition.get_address() */
      balance_id_type            id()const { return condition.get_address(); }
      /** returns 0 if asset id is not condition.asset_id */
      asset                      get_balance( asset_id_type id )const;
      bool                       is_null()const    { return balance == 0; }
      balance_record             make_null()const  { balance_record cpy(*this); cpy.balance = 0; return cpy; }

      share_type                 balance;
      withdraw_condition         condition;
      fc::time_point_sec         last_update;
   };
   typedef fc::optional<balance_record> obalance_record;

   struct asset_record
   {
      enum {
         market_issued_asset = -2
      };

      asset_record()
      :id(0),issuer_name_id(0),current_share_supply(0),maximum_share_supply(0),collected_fees(0){}

      share_type available_shares()const { return maximum_share_supply - current_share_supply; }

      bool can_issue( asset amount )const
      {
         if( id != amount.asset_id ) return false;
         return can_issue( amount.amount );
      }
      bool can_issue( share_type amount )const
      { 
         if( amount <= 0 ) return false;
         auto new_share_supply = current_share_supply + amount;
         // catch overflow conditions
         return (new_share_supply > current_share_supply) && (new_share_supply < maximum_share_supply);
      }

      bool is_null()const            { return issuer_name_id == -1; }
      /** the asset is issued by the market and not by any user */
      bool is_market_issued()const   { return issuer_name_id == market_issued_asset; }
      asset_record make_null()const { asset_record cpy(*this); cpy.issuer_name_id = -1; return cpy; }

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      fc::variant         json_data;
      name_id_type        issuer_name_id;
      fc::time_point_sec  registration_date;
      fc::time_point_sec  last_update;
      share_type          current_share_supply;
      share_type          maximum_share_supply;
      share_type          collected_fees;
   };
   typedef fc::optional<asset_record> oasset_record;

   struct name_record
   {
      name_record()
      :id(0){}

      static bool is_valid_name( const std::string& str );
      static bool is_valid_json( const std::string& str );

      bool is_null()const { return owner_key == public_key_type(); }
      name_record make_null()const    { name_record cpy(*this); cpy.owner_key = public_key_type(); return cpy;      }

      share_type delegate_pay_balance()const
      { // TODO: move to cpp
         FC_ASSERT( is_delegate() );
         return delegate_info->pay_balance;
      }
      bool    is_delegate()const { return !!delegate_info; }
      int64_t net_votes()const 
      {  // TODO: move to cpp
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_for - delegate_info->votes_against; 
      }
      void adjust_votes_for( share_type delta )
      {
         FC_ASSERT( is_delegate() );
         delegate_info->votes_for += delta;
      }
      void adjust_votes_against( share_type delta )
      {
         FC_ASSERT( is_delegate() );
         delegate_info->votes_against += delta;
      }
      share_type votes_for()const 
      {
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_for;
      }
      share_type votes_against()const 
      {
         FC_ASSERT( is_delegate() );
         return delegate_info->votes_against;
      }
      bool is_retracted()const { return active_key == public_key_type(); }

      name_id_type                 id;
      std::string                  name;
      fc::variant                  json_data;
      public_key_type              owner_key;
      public_key_type              active_key;
      fc::time_point_sec           registration_date;
      fc::time_point_sec           last_update;
      fc::optional<delegate_stats> delegate_info;
   };

   typedef fc::optional<name_record> oname_record;

   enum chain_property_enum
   {
      last_asset_id       = 0,
      last_name_id        = 1,
      last_proposal_id    = 2,
      last_random_seed_id = 3,
      chain_id            = 4 // hash of initial state
   };
   typedef uint32_t chain_property_type;

   class chain_interface
   {
      public:
         virtual ~chain_interface(){};
         /** return the timestamp from the most recent block */
         virtual fc::time_point_sec         now()const                                                      = 0;

         virtual std::vector<name_id_type>  get_active_delegates()const                                     = 0;
         bool                               is_active_delegate( name_id_type ) const;

         virtual digest_type                get_current_random_seed()const                                  = 0;

         /** return the current fee rate in millishares */
         virtual int64_t                    get_fee_rate()const                                             = 0;
         virtual int64_t                    get_delegate_pay_rate()const                                    = 0;
         virtual share_type                 get_delegate_registration_fee()const;
         virtual share_type                 get_asset_registration_fee()const;

         virtual fc::variant                get_property( chain_property_enum property_id )const            = 0;
         virtual void                       set_property( chain_property_enum property_id, 
                                                          const fc::variant& property_value )               = 0;

         virtual oasset_record              get_asset_record( asset_id_type id )const                       = 0;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const            = 0;
         virtual oname_record               get_name_record( name_id_type id )const                         = 0;
         virtual otransaction_location      get_transaction_location( const transaction_id_type& )const     = 0;
                                                                                                          
         virtual oasset_record              get_asset_record( const std::string& symbol )const              = 0;
         virtual oname_record               get_name_record( const std::string& name )const                 = 0;
                                                                                                          
         virtual void                       store_proposal_record( const proposal_record& r )               = 0;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const                 = 0;
                                                                                                          
         virtual void                       store_proposal_vote( const proposal_vote& r )                   = 0;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const              = 0;

         virtual void                       store_asset_record( const asset_record& r )                     = 0;
         virtual void                       store_balance_record( const balance_record& r )                 = 0;
         virtual void                       store_name_record( const name_record& r )                       = 0;
         virtual void                       store_transaction_location( const transaction_id_type&,
                                                                        const transaction_location& loc )   = 0;

         virtual void                       apply_deterministic_updates(){}

         virtual asset_id_type              last_asset_id()const;
         virtual asset_id_type              new_asset_id(); 

         virtual name_id_type               last_name_id()const;
         virtual name_id_type               new_name_id();

         virtual proposal_id_type           last_proposal_id()const;
         virtual proposal_id_type           new_proposal_id();
   };

   typedef std::shared_ptr<chain_interface> chain_interface_ptr;
} } // bts::blockchain

FC_REFLECT( bts::blockchain::balance_record, (balance)(condition)(last_update) )
FC_REFLECT( bts::blockchain::asset_record, (id)(symbol)(name)(description)(json_data)(issuer_name_id)(current_share_supply)(maximum_share_supply)(collected_fees)(registration_date) )
FC_REFLECT( bts::blockchain::name_record,
            (id)(name)(json_data)(owner_key)(active_key)(delegate_info)(registration_date)(last_update)
          )
FC_REFLECT( bts::blockchain::delegate_stats, 
            (votes_for)(votes_against)(blocks_produced)
            (blocks_missed)(pay_balance)(next_secret_hash)(last_block_num_produced) )
FC_REFLECT_ENUM( bts::blockchain::chain_property_enum, (last_asset_id)(last_name_id)(last_proposal_id)(last_random_seed_id)(chain_id) )
FC_REFLECT_ENUM( bts::blockchain::proposal_vote::vote_type, (no)(yes)(undefined) )
FC_REFLECT( bts::blockchain::proposal_vote, (id)(timestamp)(vote) )
FC_REFLECT( bts::blockchain::proposal_record, (id)(submitting_delegate_id)(submission_date)(subject)(body)(proposal_type)(data) )

