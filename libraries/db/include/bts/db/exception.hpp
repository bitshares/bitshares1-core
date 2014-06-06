#pragma once
#include <fc/exception/exception.hpp>
namespace bts { namespace db {
  FC_DECLARE_EXCEPTION( db_exception, 10000, "Database in Use" );
  FC_DECLARE_EXCEPTION( db_in_use_exception, 10001, "Database in Use" );
} } // bts::db
