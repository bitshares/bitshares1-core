#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariant>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>

#include <bts/rpc/rpc_server.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/client/client.hpp>
#include <bts/net/upnp.hpp>

class ClientWrapper : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool initialized READ is_initialized NOTIFY initialization_complete)
   Q_PROPERTY(QString info READ get_info NOTIFY state_changed)
   Q_PROPERTY(QString accountName READ accountName WRITE setAccountName NOTIFY accountNameChanged)
   Q_PROPERTY(QString voterAddress READ voter_address NOTIFY voter_address_changed)
   Q_PROPERTY(QString secret READ secret NOTIFY secretChanged)

public:
   ClientWrapper(QObject *parent = nullptr);
   virtual ~ClientWrapper();

   ///Not done in constructor to allow caller to connect to error()
   void initialize();
   void wait_for_initialized() {
      _init_complete.wait();
   }

   QString get_data_dir();
   Q_INVOKABLE QString get_info();

   Q_INVOKABLE void create_voter_account();
   Q_INVOKABLE QJsonObject get_voter_ballot();
   Q_INVOKABLE QJsonArray get_voter_contests();
   Q_INVOKABLE QJsonObject get_contest_by_id(QString contest_id);
   Q_INVOKABLE QJsonObject get_contest_by_id(bts::vote::digest_type contest_id);

   std::shared_ptr<bts::client::client> get_client() { return _client; }

   void handle_crash();

   bool is_initialized() const {
      return _init_complete.ready();
   }

   QString voter_address() const
   {
      return m_voterAddress;
   }

   QString secret() const
   {
      return m_secret;
   }

   QString accountName() const
   {
      return m_accountName;
   }

public Q_SLOTS:
   void set_data_dir(QString data_dir);
   void load_election();

   void begin_verification(QObject* window, QStringList verifiers, QJSValue callback);
   void get_verification_request_status(QStringList verifiers, QJSValue callback);
   void process_accepted_identity(QString identity, QJSValue callback);
   void begin_registration(QStringList registrars);
   void submit_decisions(QString json_decisions);

   void setAccountName(QString arg)
   {
      if (m_accountName == arg)
         return;

      m_accountName = arg;
      Q_EMIT accountNameChanged(arg);
   }

Q_SIGNALS:
   void initialization_complete();
   void state_changed();
   void status_update(QString statusString);
   void registered();
   void decisions_submitted();
   void error(QString errorString);

   void voter_address_changed(QString arg);

   void secretChanged(QString arg);

   void accountNameChanged(QString arg);

private:
   bts::client::config                  _cfg;
   std::shared_ptr<bts::client::client> _client;
   fc::future<void>                     _client_done;
   fc::thread*                          _main_thread;
   fc::thread                           _bitshares_thread;
   fc::future<void>                     _init_complete;
   fc::optional<fc::ip::endpoint>       _actual_httpd_endpoint;
   QSettings                            _settings;
   QTimer                               _block_interval_timer;

   std::shared_ptr<bts::net::upnp_service> _upnp_service;

   std::unordered_map<bts::vote::digest_type, bts::vote::ballot> _ballot_map;
   std::unordered_map<bts::vote::digest_type, bts::vote::contest> _contest_map;

   bts::blockchain::public_key_type lookup_public_key(QString account_name);
   std::shared_ptr<bts::rpc::rpc_client> get_rpc_client(QString account);
   Q_INVOKABLE void process_verifier_response(bts::mail::message response, QJSValue callback);
   Q_INVOKABLE void notify_acceptances(int acceptance_count, QJSValue callback);
   std::vector<bts::vote::signed_identity_property> merge_identities(const bts::vote::signed_identity& old_id,
                                                                     bts::vote::identity new_id);
   void purge_invalid_signatures(std::vector<bts::vote::signed_identity_property>& properties,
                                 bts::blockchain::address owner);
   void create_secret();

   QString m_voterAddress;
   QString m_secret;
   QString m_accountName;
};
