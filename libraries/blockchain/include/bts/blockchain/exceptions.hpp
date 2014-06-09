#pragma once
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {
   FC_DECLARE_EXCEPTION( exception, 30000, "Blockchain Exception" ); 
   FC_DECLARE_DERIVED_EXCEPTION( invalid_pts_address, bts::blockchain::exception, 30001, "Invalid PTS Address" ); 
   FC_DECLARE_DERIVED_EXCEPTION( addition_overflow,   bts::blockchain::exception, 30002, "Addition Overflow" ); 
   FC_DECLARE_DERIVED_EXCEPTION( addition_underthrow, bts::blockchain::exception, 30003, "Addition Underflow" ); 
   FC_DECLARE_DERIVED_EXCEPTION( asset_type_mismatch, bts::blockchain::exception, 30004, "Asset/Price Mismatch" ); 
   // registered in blockchain.cpp 
} }
