#pragma once
#include <memory>
#include <string>
#include <stdint.h>

#include <fc/time.hpp>

namespace bts { namespace client { 
  namespace detail {
    class bts_gntp_notifier_impl;
  }

  class bts_gntp_notifier {
  public:
    bts_gntp_notifier(const std::string& host_to_notify = "127.0.0.1", uint16_t port = 23053,
                      const std::string& bts_instance_identifier = "BitShares",
                      const fc::optional<std::string>& password = fc::optional<std::string>());
    ~bts_gntp_notifier();

    void client_is_shutting_down();
    void notify_connection_count_changed(uint32_t new_connection_count);
    void notify_client_exiting_unexpectedly();
    void notify_head_block_too_old(const fc::time_point_sec head_block_age);
  private:
    std::unique_ptr<detail::bts_gntp_notifier_impl> my;
  };
  typedef std::shared_ptr<bts_gntp_notifier> bts_gntp_notifier_ptr;

} } // end namespace bts::client
