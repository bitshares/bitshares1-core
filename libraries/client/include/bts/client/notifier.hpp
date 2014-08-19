#pragma once
#include <memory>
#include <string>
#include <stdint.h>

namespace bts { namespace client { 
  namespace detail {
    class bts_gntp_notifier_impl;
  }

  class bts_gntp_notifier {
  public:
    bts_gntp_notifier();
    ~bts_gntp_notifier();

    void connection_count_dropped_below_threshold(uint32_t current_connection_count, uint32_t notification_threshold);
    void client_exiting_unexpectedly();
  private:
    std::unique_ptr<detail::bts_gntp_notifier_impl> my;
  };
  typedef std::shared_ptr<bts_gntp_notifier> bts_gntp_notifier_ptr;

} } // end namespace bts::client