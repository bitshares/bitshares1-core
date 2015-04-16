#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/mail/message.hpp>

namespace bts { namespace client { namespace detail {

void detail::client_impl::mail_store_message(const mail::message& message)
{
   FC_ASSERT(_mail_server, "Mail server not enabled!");
   _mail_server->store(message);
}

mail::inventory_type detail::client_impl::mail_fetch_inventory(const address &owner,
                                                               const fc::time_point &start_time,
                                                               uint32_t limit) const
{
   FC_ASSERT(_mail_server, "Mail server not enabled!");
   return _mail_server->fetch_inventory(owner, start_time, limit);
}

mail::message detail::client_impl::mail_fetch_message(const mail::message_id_type& inventory_id) const
{
   FC_ASSERT(_mail_server, "Mail server not enabled!");
   return _mail_server->fetch_message(inventory_id);
}

std::multimap<mail::client::mail_status, mail::message_id_type> detail::client_impl::mail_get_processing_messages() const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_processing_messages();
}

std::multimap<mail::client::mail_status, mail::message_id_type> detail::client_impl::mail_get_archive_messages() const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_archive_messages();
}

vector<mail::email_header> detail::client_impl::mail_inbox() const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_inbox();
}

void detail::client_impl::mail_retry_send(const bts::mail::message_id_type& message_id)
{
   FC_ASSERT(_mail_client);
   _mail_client->retry_message(message_id);
}

void detail::client_impl::mail_cancel_message(const bts::mail::message_id_type &message_id)
{
   FC_ASSERT(_mail_client);
   _mail_client->cancel_message(message_id);
}

void detail::client_impl::mail_remove_message(const bts::mail::message_id_type &message_id)
{
   FC_ASSERT(_mail_client);
   _mail_client->remove_message(message_id);
}

void detail::client_impl::mail_archive_message(const bts::mail::message_id_type &message_id)
{
   FC_ASSERT(_mail_client);
   _mail_client->archive_message(message_id);
}

int detail::client_impl::mail_check_new_messages(bool get_all)
{
   FC_ASSERT(_mail_client);
   return _mail_client->check_new_messages(get_all);
}

mail::email_record detail::client_impl::mail_get_message(const mail::message_id_type& message_id) const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_message(message_id);
}

vector<mail::email_header> detail::client_impl::mail_get_messages_from(const std::string &sender) const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_messages_by_sender(sender);
}

vector<mail::email_header> detail::client_impl::mail_get_messages_to(const std::string &recipient) const
{
   FC_ASSERT(_mail_client);
   return _mail_client->get_messages_by_recipient(recipient);
}

vector<mail::email_header> detail::client_impl::mail_get_messages_in_conversation(const std::string &account_one, const std::string &account_two) const
{
   FC_ASSERT(_mail_client);
   auto forward = _mail_client->get_messages_from_to(account_one, account_two);
   auto backward = _mail_client->get_messages_from_to(account_two, account_one);

   std::move(backward.begin(), backward.end(), std::back_inserter(forward));
   std::sort(forward.begin(), forward.end(), 
             [](const bts::mail::email_header& a, const bts::mail::email_header& b) { return a.timestamp < b.timestamp;});
   return forward;
}

mail::message_id_type detail::client_impl::mail_send(const std::string& from,
                                                     const std::string& to,
                                                     const std::string& subject,
                                                     const std::string& body,
                                                     const bts::mail::message_id_type& reply_to)
{
   FC_ASSERT(_mail_client);
   return _mail_client->send_email(from, to, subject, body, reply_to);
}

} } } // namespace bts::client::detail
