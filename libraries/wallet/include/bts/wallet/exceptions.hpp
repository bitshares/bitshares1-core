#pragma once
#include <fc/exception/exception.hpp>
namespace bts { namespace wallet {
  FC_DECLARE_EXCEPTION( wallet_exception,      20000, "Wallet Error" );
  FC_DECLARE_EXCEPTION( invalid_password,      20001, "Invalid Password" );
  FC_DECLARE_EXCEPTION( login_required,        20003, "Login Required" );
  FC_DECLARE_EXCEPTION( wallet_already_exists, 20004, "Wallet Already Exists" );
  FC_DECLARE_EXCEPTION( no_such_wallet,        20005, "Wallet Does Not Exist" );
  // registered in wallet.cpp
} } // bts::wallet
