#pragma once

#include <fc/exception/exception.hpp>

namespace bts { namespace wallet {

  FC_DECLARE_EXCEPTION        ( wallet_exception,                                       20000, "wallet error" );
  FC_DECLARE_DERIVED_EXCEPTION( invalid_password,        bts::wallet::wallet_exception, 20001, "invalid password" );
  FC_DECLARE_DERIVED_EXCEPTION( login_required,          bts::wallet::wallet_exception, 20002, "login required" );
  FC_DECLARE_DERIVED_EXCEPTION( wallet_already_exists,   bts::wallet::wallet_exception, 20003, "wallet already exists" );
  FC_DECLARE_DERIVED_EXCEPTION( no_such_wallet,          bts::wallet::wallet_exception, 20004, "wallet does not exist" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_receive_account, bts::wallet::wallet_exception, 20005, "unknown receive account" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_account,         bts::wallet::wallet_exception, 20006, "unknown account" );
  FC_DECLARE_DERIVED_EXCEPTION( wallet_closed,           bts::wallet::wallet_exception, 20007, "wallet closed" );
  FC_DECLARE_DERIVED_EXCEPTION( negative_bid,            bts::wallet::wallet_exception, 20008, "negative bid" );
  FC_DECLARE_DERIVED_EXCEPTION( invalid_price,           bts::wallet::wallet_exception, 20009, "invalid price" );
  FC_DECLARE_DERIVED_EXCEPTION( insufficient_funds,      bts::wallet::wallet_exception, 20010, "insufficient funds" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_market_order,    bts::wallet::wallet_exception, 20011, "unknown market order" );
  FC_DECLARE_DERIVED_EXCEPTION( fee_greater_than_amount, bts::wallet::wallet_exception, 20012, "fee greater than amount" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_address,         bts::wallet::wallet_exception, 20013, "unknown address" );
  FC_DECLARE_DERIVED_EXCEPTION( brain_key_too_short,     bts::wallet::wallet_exception, 20014, "brain key is too short" );
  FC_DECLARE_DERIVED_EXCEPTION( password_too_short,      bts::wallet::wallet_exception, 20015, "password too short" );
  FC_DECLARE_DERIVED_EXCEPTION( invalid_format,          bts::wallet::wallet_exception, 20016, "invalid format" );
  // registered in wallet.cpp

} } // bts::wallet
