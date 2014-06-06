#pragma once
#include <fc/exception/exception.hpp>
namespace bts { namespace wallet {
  FC_DECLARE_EXCEPTION( wallet_exception, 20000, "Wallet Error" );
  FC_DECLARE_EXCEPTION( invalid_password, 20001, "Invalid Password" );
  FC_DECLARE_EXCEPTION( login_required,   20002, "Login Required" );
  // registered in wallet.cpp
} } // bts::wallet
