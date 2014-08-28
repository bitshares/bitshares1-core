#include <bts/net/stcp_socket.hpp>
#include <bts/net/chain_server.hpp>
#include <bts/net/chain_server_commands.hpp>

#include <fc/io/raw_variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/ip.hpp>

#include <thread>

namespace bts { namespace net {
    namespace detail {
        class chain_server_impl {
          public:
            chain_server* self;

            fc::tcp_server _server_socket;
            std::shared_ptr<bts::blockchain::chain_database> _chain_db;
            fc::future<void> _accept_loop_handle;
            std::set<fc::thread*> _idle_threads;
            std::set<fc::thread*> _busy_threads;

            const int _target_thread_count;
            const int _max_thread_count;

            chain_server_impl(std::shared_ptr<bts::blockchain::chain_database> chain_ptr, uint16_t port)
              : _chain_db(chain_ptr),
                _target_thread_count(std::thread::hardware_concurrency()),
                _max_thread_count(std::thread::hardware_concurrency() * 2)
            {
                _server_socket.set_reuse_address();
                _server_socket.listen(port);

                _accept_loop_handle = fc::async([this]{accept_loop();}, "accept_loop");
            }

            virtual ~chain_server_impl() {
                while (!_idle_threads.empty())
                    kill_worker(*_idle_threads.begin());
                while (!_busy_threads.empty())
                    kill_worker(*_busy_threads.begin());

                _server_socket.close();
                _accept_loop_handle.cancel_and_wait(__FUNCTION__);
            }

            void accept_loop() {
                while (!_accept_loop_handle.canceled()) {
                    fc::tcp_socket* connection_socket = new fc::tcp_socket;
                    _server_socket.accept(*connection_socket);
                    ilog("Got new connection from ${remote}", ("remote", connection_socket->remote_endpoint()));

                    auto worker_thread = get_worker();
                    if (worker_thread == nullptr) {
                        connection_socket->close();
                        delete connection_socket;
                        continue;
                    }
                    auto finished_future = worker_thread->async([=]{serve_client(connection_socket);}, "serve_client");
                    auto master_thread = &fc::thread::current();

                    finished_future.on_complete([=](fc::exception_ptr ep) { master_thread->async([=] {
                        _busy_threads.erase(worker_thread);
                        if (_idle_threads.size() < _target_thread_count) {
                            _idle_threads.insert(worker_thread);
                            ilog("Resting thread; there are now ${i} idle and ${b} busy", ("i", _idle_threads.size())("b", _busy_threads.size()));
                        } else
                            kill_worker(worker_thread);
                    }, "cleanup_chain_server_threads");});
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
              } catch (const fc::canceled_exception&) {
                  elog("We've been killed while serving a client! Hate it when that happens.");
                  connection_socket->close();
                  delete connection_socket;
              } catch (const fc::exception& e) {
                  elog("The client at ${remote} hung up on us! How rude. Error: ${e}",
                       ("remote", connection_socket->remote_endpoint())("e", e.to_detail_string()));
              }
            }

            fc::thread* get_worker()
            {
                  auto worker_thread = _idle_threads.begin();
                  if (worker_thread == _idle_threads.end()) {
                      if (_busy_threads.size() >= _max_thread_count)
                          return nullptr;
                      worker_thread = _busy_threads.insert(new fc::thread("chain_server worker")).first;
                      ilog("Creating new thread; there are now ${i} idle and ${b} busy", ("i", _idle_threads.size())("b", _busy_threads.size()));
                  } else {
                      auto thread_ptr = *worker_thread;
                      _idle_threads.erase(worker_thread);
                      worker_thread = _busy_threads.insert(thread_ptr).first;
                      ilog("Waking up idle thread; there are now ${i} idle and ${b} busy", ("i", _idle_threads.size())("b", _busy_threads.size()));
                  }
                  return *worker_thread;
            }

            void kill_worker(fc::thread* thread) {
                _idle_threads.erase(thread);
                _busy_threads.erase(thread);
                thread->quit();
                delete thread;

                ilog("Killing a thread; there are now ${i} idle and ${b} busy", ("i", _idle_threads.size())("b", _busy_threads.size()));
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
