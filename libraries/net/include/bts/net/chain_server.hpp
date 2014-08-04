#pragma once

#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace net {
    namespace detail { class chain_server_impl; }

    /**
     * @brief The chain_server class listens for remote chain_downloaders and sends them blocks when they connect.
     *
     * The chain_server will bind a port and listen on it when it is constructed. If a port is specified, the server
     * will attempt to bind that one; otherwise, the server will allow the OS to select a port. This port may be
     * queried using the get_listening_port method.
     */
    class chain_server {
        std::unique_ptr<detail::chain_server_impl> my;

      public:
        /**
         * @brief Constructs a chain_server and begins listening on the specified port.
         * @param port The port to listen on. If 0, the port is chosen by the OS.
         * @throws fc::exception If unable to bind port
         */
        chain_server(std::shared_ptr<blockchain::chain_database> chain_ptr, uint16_t port = 0);
        virtual ~chain_server();

        /**
         * @return The port this chain_server is listening on
         */
        uint16_t get_listening_port();
    };
} } //namespace bts::net
