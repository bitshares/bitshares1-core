#include <bts/net/chain_server.hpp>
#include <bts/net/chain_server_commands.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/thread/thread.hpp>

namespace bts { namespace net {
    namespace detail {
        class chain_server_impl {
          public:
            chain_server* self;

            fc::tcp_server _server_socket;
            std::shared_ptr<bts::blockchain::chain_database> _chain_db;
            fc::future<void> _accept_loop_handle;

            chain_server_impl(std::shared_ptr<bts::blockchain::chain_database> chain_ptr, uint16_t port)
              : _chain_db(chain_ptr)
            {
                _server_socket.set_reuse_address();
                _server_socket.listen(port);

                _accept_loop_handle = fc::async([this]{accept_loop();});
            }

            virtual ~chain_server_impl() {
                _accept_loop_handle.cancel_and_wait();
            }

            void accept_loop() {
                while (!_accept_loop_handle.canceled()) {
                    fc::tcp_socket connection_socket;
                    _server_socket.accept(connection_socket);
                    fc::async([&]{serve_client(connection_socket);});
                }
            }

            void handle_get_blocks_from_number(fc::tcp_socket& connection_socket) {
                uint64_t start_block;
                connection_socket >> start_block;
                if (start_block == 0) start_block = 1;
                uint64_t end_block = start_block;

                while (end_block < _chain_db->get_head_block_num()) {
                    end_block = _chain_db->get_head_block_num();
                    connection_socket << (end_block - start_block + 1);

                    for (; start_block < end_block; ++start_block) {
                        fc::raw::pack(connection_socket, fc::variant(_chain_db->get_block(start_block)));
                        fc::yield();
                    }
                }
            }

            void serve_client(fc::tcp_socket& connection_socket) {
                chain_server_commands request;
                connection_socket >> (int&)request;

                while (request != finish) {
                    switch (request) {
                      case get_blocks_from_number:
                        handle_get_blocks_from_number(connection_socket);
                        break;
                      case finish:
                        break;
                    }

                    connection_socket >> (int&)request;
                }
            }
        };
    } //namespace detail

bts::net::chain_server::chain_server(std::shared_ptr<bts::blockchain::chain_database> chain_ptr, uint16_t port)
  : my(new detail::chain_server_impl(chain_ptr, port))
{
  my->self = this;
}

chain_server::~chain_server()
{}

uint16_t bts::net::chain_server::get_listening_port()
{
    return my->_server_socket.get_port();
}

} } //namespace bts::net
