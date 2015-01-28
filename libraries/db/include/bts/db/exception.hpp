#pragma once
#include <fc/exception/exception.hpp>

namespace bts { namespace db {

FC_DECLARE_EXCEPTION( level_map_failure,            10000, "level_map failure" );
FC_DECLARE_EXCEPTION( level_map_open_failure,       10001, "level_map open failure" );

FC_DECLARE_EXCEPTION( level_pod_map_failure,        11000, "level_pod_map failure" );
FC_DECLARE_EXCEPTION( level_pod_map_open_failure,   11001, "level_pod_map open failure" );

} } // bts::db
