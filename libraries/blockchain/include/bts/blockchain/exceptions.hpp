#pragma once
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {
   FC_DECLARE_EXCEPTION( invalid_pts_address, 30001, "Invalid PTS Address" ); 
   FC_DECLARE_EXCEPTION( addition_overflow,   30002, "Addition Overflow" ); 
   FC_DECLARE_EXCEPTION( addition_underthrow, 30003, "Addition Underflow" ); 
   FC_DECLARE_EXCEPTION( asset_type_mismatch, 30004, "Asset/Price Mismatch" ); 
   // registered in blockchain.cpp 
} }
