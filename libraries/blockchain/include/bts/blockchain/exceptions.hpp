#pragma once

#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

FC_DECLARE_EXCEPTION(         blockchain_exception,                                                    30000, "Blockchain Exception" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_pts_address,              bts::blockchain::blockchain_exception, 30001, "invalid pts address" );
FC_DECLARE_DERIVED_EXCEPTION( addition_overflow,                bts::blockchain::blockchain_exception, 30002, "addition overflow" );
FC_DECLARE_DERIVED_EXCEPTION( subtraction_overflow,             bts::blockchain::blockchain_exception, 30003, "subtraction overflow" );
FC_DECLARE_DERIVED_EXCEPTION( asset_type_mismatch,              bts::blockchain::blockchain_exception, 30004, "asset/price mismatch" );
FC_DECLARE_DERIVED_EXCEPTION( unsupported_chain_operation,      bts::blockchain::blockchain_exception, 30005, "unsupported chain operation" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_transaction,              bts::blockchain::blockchain_exception, 30006, "unknown transaction" );
FC_DECLARE_DERIVED_EXCEPTION( duplicate_transaction,            bts::blockchain::blockchain_exception, 30007, "duplicate transaction" );
FC_DECLARE_DERIVED_EXCEPTION( zero_amount,                      bts::blockchain::blockchain_exception, 30008, "zero amount" );
FC_DECLARE_DERIVED_EXCEPTION( zero_price,                       bts::blockchain::blockchain_exception, 30009, "zero price" );
FC_DECLARE_DERIVED_EXCEPTION( asset_divide_by_self,             bts::blockchain::blockchain_exception, 30010, "asset divide by self" );
FC_DECLARE_DERIVED_EXCEPTION( asset_divide_by_zero,             bts::blockchain::blockchain_exception, 30011, "asset divide by zero" );
FC_DECLARE_DERIVED_EXCEPTION( new_database_version,             bts::blockchain::blockchain_exception, 30012, "new database version" );
FC_DECLARE_DERIVED_EXCEPTION( unlinkable_block,                 bts::blockchain::blockchain_exception, 30013, "unlinkable block" );
FC_DECLARE_DERIVED_EXCEPTION( price_out_of_range,               bts::blockchain::blockchain_exception, 30014, "price out of range" );
FC_DECLARE_DERIVED_EXCEPTION( block_numbers_not_sequential,     bts::blockchain::blockchain_exception, 30015, "block numbers not sequential" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_previous_block_id,        bts::blockchain::blockchain_exception, 30016, "invalid previous block" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_block_time,               bts::blockchain::blockchain_exception, 30017, "invalid block time" );
FC_DECLARE_DERIVED_EXCEPTION( time_in_past,                     bts::blockchain::blockchain_exception, 30018, "time is in the past" );
FC_DECLARE_DERIVED_EXCEPTION( time_in_future,                   bts::blockchain::blockchain_exception, 30019, "time is in the future" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_block_digest,             bts::blockchain::blockchain_exception, 30020, "invalid block digest" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_delegate_signee,          bts::blockchain::blockchain_exception, 30021, "invalid delegate signee" );
FC_DECLARE_DERIVED_EXCEPTION( failed_checkpoint_verification,   bts::blockchain::blockchain_exception, 30022, "failed checkpoint verification" );
FC_DECLARE_DERIVED_EXCEPTION( wrong_chain_id,                   bts::blockchain::blockchain_exception, 30023, "wrong chain id" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_block,                    bts::blockchain::blockchain_exception, 30024, "unknown block" );
FC_DECLARE_DERIVED_EXCEPTION( block_older_than_undo_history,    bts::blockchain::blockchain_exception, 30025, "block is older than our undo history allows us to process" );
FC_DECLARE_DERIVED_EXCEPTION( unexpected_supply_change,         bts::blockchain::blockchain_exception, 30026, "unexpected supply change" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_price,                    bts::blockchain::blockchain_exception, 30027, "invalid price" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_apr,                      bts::blockchain::blockchain_exception, 30028, "invalid apr" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_amount,                   bts::blockchain::blockchain_exception, 30029, "invalid amount" );

FC_DECLARE_EXCEPTION(         evaluation_error,                                                    31000, "Evaluation Error" );
FC_DECLARE_DERIVED_EXCEPTION( negative_deposit,                 bts::blockchain::evaluation_error, 31001, "negative deposit" );
FC_DECLARE_DERIVED_EXCEPTION( not_a_delegate,                   bts::blockchain::evaluation_error, 31002, "not a delegate" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_balance_record,           bts::blockchain::evaluation_error, 31003, "unknown balance record" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_funds,               bts::blockchain::evaluation_error, 31004, "insufficient funds" );
FC_DECLARE_DERIVED_EXCEPTION( missing_signature,                bts::blockchain::evaluation_error, 31005, "missing signature" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_claim_password,           bts::blockchain::evaluation_error, 31006, "invalid claim password" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_withdraw_condition,       bts::blockchain::evaluation_error, 31007, "invalid withdraw condition" );
FC_DECLARE_DERIVED_EXCEPTION( negative_withdraw,                bts::blockchain::evaluation_error, 31008, "negative withdraw" );
FC_DECLARE_DERIVED_EXCEPTION( not_an_active_delegate,           bts::blockchain::evaluation_error, 31009, "not an active delegate" );
FC_DECLARE_DERIVED_EXCEPTION( expired_transaction,              bts::blockchain::evaluation_error, 31010, "expired transaction" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_transaction_expiration,   bts::blockchain::evaluation_error, 31011, "invalid transaction expiration" );
FC_DECLARE_DERIVED_EXCEPTION( oversized_transaction,            bts::blockchain::evaluation_error, 31012, "transaction exceeded the maximum transaction size" );

FC_DECLARE_DERIVED_EXCEPTION( invalid_account_name,             bts::blockchain::evaluation_error, 32001, "invalid account name" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_account_id,               bts::blockchain::evaluation_error, 32002, "unknown account id" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_account_name,             bts::blockchain::evaluation_error, 32003, "unknown account name" );
FC_DECLARE_DERIVED_EXCEPTION( missing_parent_account_signature, bts::blockchain::evaluation_error, 32004, "missing parent account signature" );
FC_DECLARE_DERIVED_EXCEPTION( parent_account_retracted,         bts::blockchain::evaluation_error, 32005, "parent account retracted" );
FC_DECLARE_DERIVED_EXCEPTION( account_expired,                  bts::blockchain::evaluation_error, 32006, "account expired" );
FC_DECLARE_DERIVED_EXCEPTION( account_already_registered,       bts::blockchain::evaluation_error, 32007, "account already registered" );
FC_DECLARE_DERIVED_EXCEPTION( account_key_in_use,               bts::blockchain::evaluation_error, 32008, "account key already in use" );
FC_DECLARE_DERIVED_EXCEPTION( account_retracted,                bts::blockchain::evaluation_error, 32009, "account retracted" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_parent_account_name,      bts::blockchain::evaluation_error, 32010, "unknown parent account name" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_delegate_slate,           bts::blockchain::evaluation_error, 32011, "unknown delegate slate" );
FC_DECLARE_DERIVED_EXCEPTION( too_may_delegates_in_slate,       bts::blockchain::evaluation_error, 32012, "too many delegates in slate" );
FC_DECLARE_DERIVED_EXCEPTION( pay_balance_remaining,            bts::blockchain::evaluation_error, 32013, "pay balance remaining" );

FC_DECLARE_DERIVED_EXCEPTION( not_a_delegate_signature,         bts::blockchain::evaluation_error, 33002, "not delegates signature" );

FC_DECLARE_DERIVED_EXCEPTION( invalid_precision,                bts::blockchain::evaluation_error, 35001, "invalid precision" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_symbol,             bts::blockchain::evaluation_error, 35002, "invalid asset symbol" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_asset_id,                 bts::blockchain::evaluation_error, 35003, "unknown asset id" );
FC_DECLARE_DERIVED_EXCEPTION( asset_symbol_in_use,              bts::blockchain::evaluation_error, 35004, "asset symbol in use" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_amount,             bts::blockchain::evaluation_error, 35005, "invalid asset amount" );
FC_DECLARE_DERIVED_EXCEPTION( negative_issue,                   bts::blockchain::evaluation_error, 35006, "negative issue" );
FC_DECLARE_DERIVED_EXCEPTION( over_issue,                       bts::blockchain::evaluation_error, 35007, "over issue" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_asset_symbol,             bts::blockchain::evaluation_error, 35008, "unknown asset symbol" );
FC_DECLARE_DERIVED_EXCEPTION( asset_id_in_use,                  bts::blockchain::evaluation_error, 35009, "asset id in use" );
FC_DECLARE_DERIVED_EXCEPTION( not_user_issued,                  bts::blockchain::evaluation_error, 35010, "not user issued" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_name,               bts::blockchain::evaluation_error, 35011, "invalid asset name" );
FC_DECLARE_DERIVED_EXCEPTION( amount_too_large,                 bts::blockchain::evaluation_error, 35012, "amount too large" );
FC_DECLARE_DERIVED_EXCEPTION( outstanding_shares_exist,         bts::blockchain::evaluation_error, 35013, "outstanding shares exist" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_fee_rate,                 bts::blockchain::evaluation_error, 35014, "invalid fee rate" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_authority,                bts::blockchain::evaluation_error, 35015, "invalid authority" );
FC_DECLARE_DERIVED_EXCEPTION( permission_not_available,         bts::blockchain::evaluation_error, 35016, "permission not available" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_whitelist,                bts::blockchain::evaluation_error, 35017, "invalid whitelist" );
FC_DECLARE_DERIVED_EXCEPTION( not_on_whitelist,                 bts::blockchain::evaluation_error, 35018, "not on whitelist" );
FC_DECLARE_DERIVED_EXCEPTION( market_halted,                    bts::blockchain::evaluation_error, 35019, "market halted" );

FC_DECLARE_DERIVED_EXCEPTION( delegate_vote_limit,              bts::blockchain::evaluation_error, 36001, "delegate_vote_limit" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_fee,                 bts::blockchain::evaluation_error, 36002, "insufficient fee" );
FC_DECLARE_DERIVED_EXCEPTION( negative_fee,                     bts::blockchain::evaluation_error, 36003, "negative fee" );
FC_DECLARE_DERIVED_EXCEPTION( missing_deposit,                  bts::blockchain::evaluation_error, 36004, "missing deposit" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_relay_fee,           bts::blockchain::evaluation_error, 36005, "insufficient relay fee" );
FC_DECLARE_DERIVED_EXCEPTION( fee_greater_than_max,             bts::blockchain::evaluation_error, 36006, "fee greater than max" );

FC_DECLARE_DERIVED_EXCEPTION( invalid_market,                   bts::blockchain::evaluation_error, 37001, "invalid market" );
FC_DECLARE_DERIVED_EXCEPTION( unknown_market_order,             bts::blockchain::evaluation_error, 37002, "unknown market order" );
FC_DECLARE_DERIVED_EXCEPTION( shorting_base_shares,             bts::blockchain::evaluation_error, 37003, "shorting base shares" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_collateral,          bts::blockchain::evaluation_error, 37004, "insufficient collateral" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_depth,               bts::blockchain::evaluation_error, 37005, "insufficient depth" );
FC_DECLARE_DERIVED_EXCEPTION( insufficient_feeds,               bts::blockchain::evaluation_error, 37006, "insufficient feeds" );
FC_DECLARE_DERIVED_EXCEPTION( invalid_feed_price,               bts::blockchain::evaluation_error, 37007, "invalid feed price" );

FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_overflow,    bts::blockchain::evaluation_error, 38001, "price multiplication overflow" );
FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_underflow,   bts::blockchain::evaluation_error, 38002, "price multiplication underflow" );
FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_undefined,   bts::blockchain::evaluation_error, 38003, "price multiplication undefined product 0*inf" );

} } // bts::blockchain
