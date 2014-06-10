#pragma once
#include <fc/exception/exception.hpp>
namespace bts { namespace wallet {
  FC_DECLARE_EXCEPTION( wallet_exception,        20000, "wallet error" );
  FC_DECLARE_DERIVED_EXCEPTION( invalid_password,        bts::wallet::wallet_exception, 20001, "invalid password" );
  FC_DECLARE_DERIVED_EXCEPTION( login_required,          bts::wallet::wallet_exception, 20003, "login required" );
  FC_DECLARE_DERIVED_EXCEPTION( wallet_already_exists,   bts::wallet::wallet_exception, 20004, "wallet already exists" );
  FC_DECLARE_DERIVED_EXCEPTION( no_such_wallet,          bts::wallet::wallet_exception, 20005, "wallet does not exist" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_receive_account, bts::wallet::wallet_exception, 20006, "unknown receive account" );
  FC_DECLARE_DERIVED_EXCEPTION( unknown_account,         bts::wallet::wallet_exception, 20007, "unknown account" );
  // registered in wallet.cpp
} } // bts::wallet
