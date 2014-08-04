#pragma once

/**
 * This is an internal header for the bitshares toolkit. It does not contain any classes or functions intended for
 * clients of the toolkit. It exists purely as an implementation detail, and may change at any time withotu notice.
 */

#include <fc/reflect/reflect.hpp>

namespace bts { namespace net {
    enum chain_server_commands {
        get_blocks_from_number,
        finish
    };
} } //namespace bts::net

FC_REFLECT_ENUM(bts::net::chain_server_commands, (get_blocks_from_number))
