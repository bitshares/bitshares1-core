#pragma once

#include <bts/blockchain/block.hpp>

#include <fc/thread/future.hpp>
#include <fc/network/ip.hpp>

#include <functional>

namespace bts { namespace net {
    namespace detail { class chain_downloader_impl; }

    /**
     * @brief The chain_downloader class downloads all blocks after a given number from a remote chain_server
     *
     * Before blocks can be downloaded, one or more endpoints which point to chain_servers must be provided. To start
     * the download, call get_blocks and provide a callback function which will handle the new blocks, and optionally
     * the block number to start downloading from.
     */
    class chain_downloader {
        std::unique_ptr<detail::chain_downloader_impl> my;

      public:
        /**
         * @brief Constructor
         * @param chain_servers An initial set of chain_server endpoints. Passing a parameter here is the same as
         * calling add_chain_servers(chain_servers)
         */
        chain_downloader(std::vector<fc::ip::endpoint> chain_servers = std::vector<fc::ip::endpoint>());
        virtual ~chain_downloader();

        /**
         * @brief Add a single chain_server to the list of chain servers
         * @param server The network address and port of the server to add
         */
        void add_chain_server(const fc::ip::endpoint& server);
        /**
         * @brief Add several chain_servers to the list of chain servers
         * @param servers Vector of chain_server endpoints
         */
        void add_chain_servers(const std::vector<fc::ip::endpoint>& servers);

        /**
         * @brief Asynchronously retrieve all new blocks from one of the available chain_server nodes
         * @param new_block_callback Callback function taking the newly downloaded block and the count of blocks remaining
         * @param first_block_number The first block number to download. Defaults to 0, which means to download all
         * blocks in chain.
         * @return A future monitoring the function downloading blocks. When this future completes, all blocks have
         * been downloaded.
         *
         * If new_block_callback is unset, a valid future is still returned, but nothing will be done and the
         * function monitored by the future will return immediately.
         */
        fc::future<void> get_all_blocks(std::function<void (const blockchain::full_block&, uint32_t)> new_block_callback,
                                        uint32_t first_block_number);
    };
} } //namespace bts::net
