#pragma once

#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

struct immutable_chain_parameters
{
   uint16_t min_committee_member_count = 11;
   uint16_t min_witness_count = 101;
};

struct genesis_state_type {
   struct initial_account_type {
      initial_account_type(const string& name = string(),
                           const public_key_type& owner_key = public_key_type(),
                           const public_key_type& active_key = public_key_type(),
                           bool is_lifetime_member = false)
         : name(name),
           owner_key(owner_key),
           active_key(active_key == public_key_type()? owner_key : active_key),
           is_lifetime_member(is_lifetime_member)
      {}
      uint32_t id;
      string name;
      public_key_type owner_key;
      public_key_type active_key;
      bool is_lifetime_member;
      bool is_prefixed;
   };
   struct initial_asset_type {
      struct initial_collateral_position {
         address owner;
         share_type collateral;
         share_type debt;
      };

      uint32_t id;

      string symbol;
      string issuer_name;

      string description;
      uint8_t precision;

      share_type max_supply;
      share_type accumulated_fees;

      bool is_bitasset = false;
      vector<initial_collateral_position> collateral_records;
   };
   struct initial_balance_type {
      address owner;
      string asset_symbol;
      share_type amount;
   };
   struct initial_vesting_balance_type {
      address owner;
      string asset_symbol;
      share_type amount;
      time_point_sec begin_timestamp;
      uint32_t vesting_duration_seconds = 0;
      share_type begin_balance;
   };
   struct initial_witness_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
      public_key_type block_signing_key;
   };
   struct initial_committee_member_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
   };
   struct initial_worker_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
      share_type daily_pay;
   };

   time_point_sec                           initial_timestamp;
   share_type                               max_core_supply = 0;
   immutable_chain_parameters               immutable_parameters;
   vector<initial_account_type>             initial_accounts;
   vector<initial_asset_type>               initial_assets;
   vector<initial_balance_type>             initial_balances;
   vector<initial_vesting_balance_type>     initial_vesting_balances;
   int                                      initial_active_witnesses = 101;
   vector<initial_witness_type>             initial_witness_candidates;
   vector<initial_committee_member_type>    initial_committee_candidates;
   vector<initial_worker_type>              initial_worker_candidates;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::immutable_chain_parameters,
   (min_committee_member_count)
   (min_witness_count)
)

FC_REFLECT(bts::blockchain::genesis_state_type::initial_account_type, (id)(name)(owner_key)(active_key)(is_lifetime_member)(is_prefixed))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_asset_type,
           (id)(symbol)(issuer_name)(description)(precision)(max_supply)(accumulated_fees)(is_bitasset)(collateral_records))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_asset_type::initial_collateral_position,
           (owner)(collateral)(debt))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_balance_type,
           (owner)(asset_symbol)(amount))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_vesting_balance_type,
           (owner)(asset_symbol)(amount)(begin_timestamp)(vesting_duration_seconds)(begin_balance))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_witness_type, (owner_name)(block_signing_key))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_committee_member_type, (owner_name))

FC_REFLECT(bts::blockchain::genesis_state_type::initial_worker_type, (owner_name)(daily_pay))

FC_REFLECT(bts::blockchain::genesis_state_type,
           (initial_timestamp)(max_core_supply)(initial_accounts)(initial_assets)(initial_balances)
           (initial_vesting_balances)(initial_active_witnesses)(initial_witness_candidates)
           (initial_committee_candidates)(initial_worker_candidates)
           (immutable_parameters))
