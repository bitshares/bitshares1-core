#include <bts/net/stcp_socket.hpp>
#include <bts/net/chain_server.hpp>
#include <bts/net/chain_server_commands.hpp>

#include <fc/io/raw_variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/ip.hpp>

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
                    fc::tcp_socket* connection_socket = new fc::tcp_socket;
                    _server_socket.accept(*connection_socket);
                    ilog("Got new connection from ${remote}", ("remote", connection_socket->remote_endpoint()));
                    fc::async([=]{serve_client(connection_socket);});
                }
            }

            void handle_get_blocks_from_number(fc::tcp_socket& connection_socket) {
              try {
                uint32_t start_block;
                fc::raw::unpack(connection_socket, start_block);
                if (start_block == 0) start_block = 1;
                uint32_t end_block = start_block;

                while (end_block <= _chain_db->get_head_block_num()) {
                    end_block = _chain_db->get_head_block_num();
                    auto blocks_to_send = end_block - start_block + 1;
                    fc::raw::pack(connection_socket, blocks_to_send);

                    ilog("Sending blocks from ${start} to ${finish} to ${remote}",
                         ("start", start_block)("finish", end_block)("remote", connection_socket.remote_endpoint()));
                    for (; start_block <= end_block; ++start_block) {
                        fc::raw::pack(connection_socket, _chain_db->get_block(start_block));
                        if (start_block % 10 == 0)
                            fc::yield();
                    }
                    end_block = start_block;
                }

                // Now sending zero more blocks...
                fc::raw::pack(connection_socket, uint32_t(0));
              } FC_RETHROW_EXCEPTIONS(error, "", ("remote_endpoint", connection_socket.remote_endpoint()))
            }

            void serve_client(fc::tcp_socket* connection_socket) {
              try {
                FC_ASSERT(connection_socket->is_open());
                fc::raw::pack(*connection_socket, PROTOCOL_VERSION);

                chain_server_commands request;
                fc::raw::unpack(*connection_socket, request);

                while (request != finish) {
                    ilog("Got ${req} request from ${remote}", ("req", request)("remote", connection_socket->remote_endpoint()));
                    switch (request) {
                      case get_blocks_from_number:
                        handle_get_blocks_from_number(*connection_socket);
                        break;
                      case finish:
                        break;
                    }

                    fc::raw::unpack(*connection_socket, request);
                }

                ilog("Closing connection to ${remote}", ("remote", connection_socket->remote_endpoint()));
                connection_socket->close();
                delete connection_socket;
              } catch (const fc::exception& e) {
                  elog("The client at ${remote} hung up on us! How rude. Error: ${e}",
                       ("remote", connection_socket->remote_endpoint())("e", e.to_detail_string()));
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
{
  my->_server_socket.close();
  my->_accept_loop_handle.cancel_and_wait();
}

uint16_t bts::net::chain_server::get_listening_port()
{
    return my->_server_socket.get_port();
}

} } //namespace bts::net
