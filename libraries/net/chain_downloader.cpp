#include <bts/net/chain_downloader.hpp>
#include <bts/net/chain_server_commands.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/thread/thread.hpp>

namespace bts { namespace net {

    namespace detail {
      class chain_downloader_impl {
        public:
          chain_downloader* self;

          std::unique_ptr<fc::tcp_socket> _client_socket;
          std::vector<fc::ip::endpoint> _chain_servers;

          void connect_to_chain_server()
          { try {
              auto next_server = _chain_servers.begin();
              _client_socket = std::unique_ptr<fc::tcp_socket>(new fc::tcp_socket);
              while(!_client_socket->is_open() && next_server != _chain_servers.end()) {
                  _client_socket = std::unique_ptr<fc::tcp_socket>(new fc::tcp_socket);
                  try {
                      _client_socket->connect_to(*(next_server++));
                  } catch (const fc::exception& e) {
                      wlog("Failed to connect to chain_server: ${e}", ("e", e.to_detail_string()));
                      _client_socket->close();
                      continue;
                  }

                  uint32_t protocol_version = -1;
                  fc::raw::unpack(*_client_socket, protocol_version);
                  if (protocol_version != PROTOCOL_VERSION) {
                      wlog("Can't talk to chain server; he's using protocol ${srv} and I'm using ${cli}!",
                           ("srv", protocol_version)("cli", PROTOCOL_VERSION));
                      fc::raw::pack(*_client_socket, finish);
                      _client_socket->close();
                  }
              }
          } FC_RETHROW_EXCEPTIONS(error, "") }

          void get_all_blocks(std::function<void (const blockchain::full_block&)> new_block_callback,
                              uint32_t first_block_number)
          { try {
              if (!new_block_callback)
                  return;

              auto start_time = fc::time_point::now();

              connect_to_chain_server();
              FC_ASSERT(_client_socket->is_open(), "unable to connect to any chain server");
              ilog("Connected to ${remote}; requesting blocks after ${num}",
                   ("remote", _client_socket->remote_endpoint())("num", first_block_number));

              fc::raw::pack(*_client_socket, get_blocks_from_number);
              fc::raw::pack(*_client_socket, first_block_number);

              uint32_t blocks_to_retrieve = 0;
              uint32_t blocks_in = 0;
              fc::raw::unpack(*_client_socket, blocks_to_retrieve);
              ilog("Server at ${remote} is sending us ${num} blocks.",
                   ("remote", _client_socket->remote_endpoint())("num", blocks_to_retrieve));

              while(blocks_to_retrieve > 0)
              {
                  blockchain::full_block block;
                  fc::raw::unpack(*_client_socket, block);

                  new_block_callback(block);
                  --blocks_to_retrieve;
                  ++blocks_in;

                  if(blocks_to_retrieve == 0) {
                      fc::raw::unpack(*_client_socket, blocks_to_retrieve);
                      if(blocks_to_retrieve > 0)
                          ilog("Server at ${remote} is sending us ${num} blocks.",
                               ("remote", _client_socket->remote_endpoint())("num", blocks_to_retrieve));
                  }
              }

              ulog("Finished fast-syncing ${num} blocks at ${rate} blocks/sec.",
                   ("num", blocks_in)("rate", blocks_in/((fc::time_point::now() - start_time).count() / 1000000.0)));
              ilog("Finished getting ${num} blocks from ${remote}",
                   ("num", blocks_in)("remote", _client_socket->remote_endpoint()));
              fc::raw::pack(*_client_socket, finish);
            } FC_RETHROW_EXCEPTIONS(error, "", ("first_block_number", first_block_number))
          }
      };
    } //namespace detail

    bts::net::chain_downloader::chain_downloader(std::vector<fc::ip::endpoint> chain_servers)
      : my(new detail::chain_downloader_impl)
    {
        my->self = this;
        add_chain_servers(chain_servers);
    }

    bts::net::chain_downloader::~chain_downloader()
    {}

    void chain_downloader::add_chain_server(const fc::ip::endpoint& server)
    {
        if(std::find(my->_chain_servers.begin(), my->_chain_servers.end(), server) == my->_chain_servers.end())
            my->_chain_servers.push_back(server);
    }

    void chain_downloader::add_chain_servers(const std::vector<fc::ip::endpoint>& servers)
    {
        my->_chain_servers.reserve(my->_chain_servers.size() + servers.size());

        for (auto server : servers)
            add_chain_server(server);

        my->_chain_servers.shrink_to_fit();
    }

    fc::future<void> chain_downloader::get_all_blocks(std::function<void (const blockchain::full_block&)> new_block_callback,
                                                      uint32_t first_block_number)
    {
        return fc::async([=]{my->get_all_blocks(new_block_callback, first_block_number);});
    }

  } } //namespace bts::blockchain
