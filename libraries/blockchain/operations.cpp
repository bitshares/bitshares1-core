#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/edge_operations.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/object_operations.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/slate_operations.hpp>

#include <fc/io/raw_variant.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {
   const operation_type_enum withdraw_operation::type               = withdraw_op_type;
   const operation_type_enum deposit_operation::type                = deposit_op_type;

   const operation_type_enum register_account_operation::type       = register_account_op_type;
   const operation_type_enum update_account_operation::type         = update_account_op_type;
   const operation_type_enum withdraw_pay_operation::type           = withdraw_pay_op_type;

   const operation_type_enum create_asset_operation::type           = create_asset_op_type;
   const operation_type_enum update_asset_operation::type           = update_asset_op_type;
   const operation_type_enum issue_asset_operation::type            = issue_asset_op_type;
   const operation_type_enum create_asset_proposal::type            = create_asset_prop_op_type;

   const operation_type_enum bid_operation::type                    = bid_op_type;
   const operation_type_enum ask_operation::type                    = ask_op_type;
   const operation_type_enum short_operation::type                  = short_op_type;
   const operation_type_enum cover_operation::type                  = cover_op_type;
   const operation_type_enum add_collateral_operation::type         = add_collateral_op_type;
   const operation_type_enum update_cover_operation::type      = update_cover_op_type;

   const operation_type_enum define_slate_operation::type           = define_slate_op_type;

   const operation_type_enum update_feed_operation::type            = update_feed_op_type;

   const operation_type_enum burn_operation::type                   = burn_op_type;

   const operation_type_enum release_escrow_operation::type         = release_escrow_op_type;

   const operation_type_enum update_signing_key_operation::type     = update_signing_key_op_type;

   const operation_type_enum relative_bid_operation::type           = relative_bid_op_type;
   const operation_type_enum relative_ask_operation::type           = relative_ask_op_type;

   const operation_type_enum update_balance_vote_operation::type    = update_balance_vote_op_type;

   const operation_type_enum set_object_operation::type             = set_object_op_type;
   const operation_type_enum authorize_operation::type              = authorize_op_type;

   const operation_type_enum update_asset_ext_operation::type       = update_asset_ext_op_type;

   const operation_type_enum set_edge_operation::type               = set_edge_op_type;
   const operation_type_enum pay_fee_operation::type               = pay_fee_op_type;

   static bool first_chain = []()->bool{
      bts::blockchain::operation_factory::instance().register_operation<withdraw_operation>();
      bts::blockchain::operation_factory::instance().register_operation<deposit_operation>();

      bts::blockchain::operation_factory::instance().register_operation<register_account_operation>();
      bts::blockchain::operation_factory::instance().register_operation<update_account_operation>();
      bts::blockchain::operation_factory::instance().register_operation<withdraw_pay_operation>();

      bts::blockchain::operation_factory::instance().register_operation<create_asset_operation>();
      bts::blockchain::operation_factory::instance().register_operation<update_asset_operation>();
      bts::blockchain::operation_factory::instance().register_operation<issue_asset_operation>();

      bts::blockchain::operation_factory::instance().register_operation<bid_operation>();
      bts::blockchain::operation_factory::instance().register_operation<ask_operation>();
      bts::blockchain::operation_factory::instance().register_operation<short_operation>();
      bts::blockchain::operation_factory::instance().register_operation<cover_operation>();
      bts::blockchain::operation_factory::instance().register_operation<add_collateral_operation>();
      bts::blockchain::operation_factory::instance().register_operation<update_cover_operation>();

      bts::blockchain::operation_factory::instance().register_operation<define_slate_operation>();

      bts::blockchain::operation_factory::instance().register_operation<update_feed_operation>();

      bts::blockchain::operation_factory::instance().register_operation<burn_operation>();

      bts::blockchain::operation_factory::instance().register_operation<release_escrow_operation>();

      bts::blockchain::operation_factory::instance().register_operation<update_signing_key_operation>();

      bts::blockchain::operation_factory::instance().register_operation<relative_bid_operation>();
      bts::blockchain::operation_factory::instance().register_operation<relative_ask_operation>();

      bts::blockchain::operation_factory::instance().register_operation<update_balance_vote_operation>();

      bts::blockchain::operation_factory::instance().register_operation<set_object_operation>();
      bts::blockchain::operation_factory::instance().register_operation<authorize_operation>();

      bts::blockchain::operation_factory::instance().register_operation<update_asset_ext_operation>();
      bts::blockchain::operation_factory::instance().register_operation<create_asset_proposal>();

      bts::blockchain::operation_factory::instance().register_operation<set_edge_operation>();
      bts::blockchain::operation_factory::instance().register_operation<pay_fee_operation>();

      return true;
   }();

   operation_factory& operation_factory::instance()
   {
      static std::unique_ptr<operation_factory> inst( new operation_factory() );
      return *inst;
   }

   void operation_factory::to_variant( const bts::blockchain::operation& in, fc::variant& output )
   { try {
      auto converter_itr = _converters.find( in.type.value );
      FC_ASSERT( converter_itr != _converters.end() );
      converter_itr->second->to_variant( in, output );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void operation_factory::from_variant( const fc::variant& in, bts::blockchain::operation& output )
   { try {
      auto obj = in.get_object();
      output.type = obj["type"].as<operation_type_enum>();

      auto converter_itr = _converters.find( output.type.value );
      FC_ASSERT( converter_itr != _converters.end() );
      converter_itr->second->from_variant( in, output );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("in",in) ) }

} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::operation& var,  variant& vo )
   {
      bts::blockchain::operation_factory::instance().to_variant( var, vo );
   }

   void from_variant( const variant& var,  bts::blockchain::operation& vo )
   {
      bts::blockchain::operation_factory::instance().from_variant( var, vo );
   }
}
