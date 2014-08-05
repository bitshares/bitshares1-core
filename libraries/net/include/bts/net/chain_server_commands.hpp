#pragma once

/**
 * This is an internal header for the bitshares toolkit. It does not contain any classes or functions intended for
 * clients of the toolkit. It exists purely as an implementation detail, and may change at any time withotu notice.
 */

#include <fc/reflect/reflect.hpp>

const static uint32_t PROTOCOL_VERSION = 0;

namespace bts { namespace net { namespace detail {
    enum chain_server_commands {
        finish = 0,
        get_blocks_from_number
    };
} } } //namespace bts::net::detail

FC_REFLECT_ENUM(bts::net::detail::chain_server_commands, (get_blocks_from_number))
FC_REFLECT_TYPENAME(bts::net::detail::chain_server_commands)
