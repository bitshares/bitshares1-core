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
     *
     * The chain_server responds to chain_server_commands. When a client connects, the server first sends it the
     * protocol version this server expects. If the client does not understand that version, it must send a finish
     * command and terminate the connection. Otherwise, the client should send a command along with any arguments
     * that command requires, complete the communication for that command, then repeat with another command and
     * so-on. When the client is finished, it should send the finish command, to indicate that  it is ready to
     * terminate the connection.
     *
     * The commands are as follows:
     * * get_blocks_from_number
     *      This command takes one argument, the number of the first block to retrieve. The server responds with a
     *      uint32_t count of blocks it will send, followed by the blocks, which are encoded as packed variants of
     *      full_block objects. When the server has finished sending these blocks, it repeats the procedure for
     *      any new blocks which have been made in the interim, so another count is sent, followed by that number
     *      of blocks. When the server sends a count of 0, there are no blocks, and the command is complete.
     *
     * All block numbers are of type uint32_t
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
