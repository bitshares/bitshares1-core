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

          fc::tcp_socket _client_socket;
          std::vector<fc::ip::endpoint> _chain_servers;

          fc::tcp_socket connect_to_chain_server()
          {
              auto next_server = _chain_servers.begin();
              while(!_client_socket.is_open() && next_server != _chain_servers.end()) {
                  _client_socket = fc::tcp_socket();
                  _client_socket.connect_to(*(next_server++));

                  uint32_t protocol_version = -1;
                  _client_socket >> protocol_version;
                  if (protocol_version != PROTOCOL_VERSION) {
                      wlog("Can't talk to chain server; he's using protocol ${srv} and I'm using ${cli}!",
                           ("srv", protocol_version)("cli", PROTOCOL_VERSION));
                      _client_socket << finish;
                      _client_socket.close();
                  }
              }

              return _client_socket;
          }

          void get_all_blocks(std::function<void (const blockchain::full_block&)> new_block_callback,
                              uint64_t first_block_number) {
              if (!new_block_callback)
                  return;

              connect_to_chain_server();
              FC_ASSERT(_client_socket.is_open(), "unable to connect to any chain server");
              ilog("Connected to ${remote}", ("remote", _client_socket.remote_endpoint()));

              _client_socket << get_blocks_from_number << first_block_number;

              uint64_t blocks_to_retrieve = 0;
              _client_socket >> blocks_to_retrieve;
              ilog("Server at ${remote} is sending us ${num} blocks.",
                   ("remote", _client_socket.remote_endpoint())("num", blocks_to_retrieve));

              while(blocks_to_retrieve > 0)
              {
                  fc::variant block;
                  fc::raw::unpack(_client_socket, block);

                  new_block_callback(block.as<bts::blockchain::full_block>());
                  --blocks_to_retrieve;

                  if(blocks_to_retrieve == 0) {
                      _client_socket >> blocks_to_retrieve;
                      if(blocks_to_retrieve > 0)
                          ilog("Server at ${remote} is sending us ${num} blocks.",
                               ("remote", _client_socket.remote_endpoint())("num", blocks_to_retrieve));
                  }
              }

              _client_socket << finish;
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
                                                      uint64_t first_block_number)
    {
        return fc::async([&]{my->get_all_blocks(new_block_callback, first_block_number);});
    }

  } } //namespace bts::blockchain
