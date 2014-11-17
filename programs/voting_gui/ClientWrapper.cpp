#include "ClientWrapper.hpp"

#include <bts/cli/pretty.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/net/upnp.hpp>
#include <bts/net/config.hpp>
#include <bts/db/exception.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/vote/messages.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <QGuiApplication>
#include <QResource>
#include <QSettings>
#include <QJsonDocument>
#include <QUrl>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

#include <iostream>

using std::string;

ClientWrapper::ClientWrapper(QObject *parent)
   : QObject(parent),
     _main_thread(&fc::thread::current()),
     _bitshares_thread("bitshares"),
     _settings("BitShares", BTS_BLOCKCHAIN_NAME)
{
   _block_interval_timer.setInterval(1000);
   connect(&_block_interval_timer, &QTimer::timeout, this, &ClientWrapper::state_changed);
   _block_interval_timer.start();
}

ClientWrapper::~ClientWrapper()
{
   _init_complete.wait();
   QSettings("BitShares", BTS_BLOCKCHAIN_NAME).setValue("crash_state", "no_crash");
   if (_client)
      _bitshares_thread.async([this]{
         _client->stop();
         _client_done.wait();
         _client.reset();
      }).wait();
}

void ClientWrapper::handle_crash()
{
   auto response = QMessageBox::question(nullptr,
                                         tr("Crash Detected"),
                                         tr("It appears that %1 crashed last time it was running. "
                                            "If this is happening frequently, it could be caused by a "
                                            "corrupted database. Would you like "
                                            "to reset the database (this will take several minutes) or "
                                            "to continue normally? Resetting the database will "
                                            "NOT lose any of your information or funds.").arg(qApp->applicationName()),
                                         tr("Reset Database"),
                                         tr("Continue Normally"),
                                         QString(), 1);
   if (response == 0)
      QDir((get_data_dir() + "/chain")).removeRecursively();
}

QString ClientWrapper::get_data_dir()
{
   QString data_dir = QString::fromStdWString(bts::client::get_data_dir(boost::program_options::variables_map()).generic_wstring());
   if (_settings.contains("data_dir"))
      data_dir = _settings.value("data_dir").toString();
   int data_dir_index = qApp->arguments().indexOf("--data-dir");
   if (data_dir_index != -1 && qApp->arguments().size() > data_dir_index+1)
      data_dir = qApp->arguments()[data_dir_index+1];

   return data_dir;
}

void ClientWrapper::initialize()
{
   bool upnp = _settings.value( "network/p2p/use_upnp", true ).toBool();

   std::string default_wallet_name = _settings.value("client/default_wallet_name", "default").toString().toStdString();
   _settings.setValue("client/default_wallet_name", QString::fromStdString(default_wallet_name));

#ifdef _WIN32
   _cfg.rpc.rpc_user = "";
   _cfg.rpc.rpc_password = "";
#else
   _cfg.rpc.rpc_user     = "randomuser";
   _cfg.rpc.rpc_password = fc::variant(fc::ecc::private_key::generate()).as_string();
#endif
   _cfg.rpc.httpd_endpoint = fc::ip::endpoint::from_string( "127.0.0.1:9999" );
   _cfg.rpc.httpd_endpoint.set_port(0);
   ilog( "config: ${d}", ("d", fc::json::to_pretty_string(_cfg) ) );

   auto data_dir = get_data_dir();
   wlog("Starting client with data-dir: ${ddir}", ("ddir", fc::path(data_dir.toStdWString())));

   _init_complete = _bitshares_thread.async( [=](){
      try
      {
         _main_thread->async( [&]{ Q_EMIT status_update(tr("Starting %1").arg(qApp->applicationName())); });
         _client = std::make_shared<bts::client::client>("qt_wallet");
         _client->open( data_dir.toStdWString(), fc::optional<fc::path>(), [=](float progress) {
            _main_thread->async( [=]{ Q_EMIT status_update(tr("Reindexing database... Approximately %1% complete.").arg(progress, 0, 'f', 0)); } );
         } );

         if(!_client->get_wallet()->is_enabled())
            _main_thread->async([&]{ Q_EMIT error(tr("Wallet is disabled in your configuration file. Please enable the wallet and relaunch the application.")); });

         _main_thread->async( [&]{ Q_EMIT status_update(tr("Loading...")); });
         _client->configure( data_dir.toStdWString() );
         _client->init_cli();

         _main_thread->async( [&]{ Q_EMIT status_update(tr("Connecting to %1 network").arg(qApp->applicationName())); });
         _client->listen_on_port(0, false /*don't wait if not available*/);
         fc::ip::endpoint actual_p2p_endpoint = _client->get_p2p_listening_endpoint();

         _client->set_daemon_mode(true);
         _client_done = _client->start();
         if( !_actual_httpd_endpoint )
         {
            _main_thread->async( [&]{ Q_EMIT error( tr("Unable to start HTTP server...")); });
         }

         _client->start_networking([=]{
            for (std::string default_peer : _cfg.default_peers)
               _client->connect_to_peer(default_peer);
         });

         if( upnp )
         {
            _upnp_service = std::make_shared<bts::net::upnp_service>();
            _upnp_service->map_port( actual_p2p_endpoint.port() );
         }

         //FIXME: nuff sed
         std::string passphrase = "voting_booth_passphrase_is_totally_secure";
         try
         {
            _client->wallet_open(default_wallet_name);
         }
         catch( bts::wallet::no_such_wallet )
         {
            _client->wallet_create(default_wallet_name, passphrase);
         }
         _client->wallet_unlock(99999999, passphrase);
      }
      catch (const bts::db::db_in_use_exception&)
      {
         _main_thread->async( [&]{ Q_EMIT error( tr("An instance of %1 is already running! Please close it and try again.").arg(qApp->applicationName())); });
         qDebug() << "Startup failed because database is locked.";
      }
      catch (...)
      {
         ilog("Failure when attempting to initialize client");
         qDebug() << "Startup failed, unknown reason.";
         if (fc::exists(fc::path(data_dir.toStdWString()) / "chain")) {
            fc::remove_all(fc::path(data_dir.toStdWString()) / "chain");
            _main_thread->async( [&]{ Q_EMIT error( tr("An error occurred while trying to start. Please try restarting the application.")); });
         } else
            _main_thread->async( [&]{ Q_EMIT error( tr("An error occurred while trying to start.")); });
      }
   });
   _init_complete.on_complete([this](fc::exception_ptr) {
      qDebug() << "Initialization complete.";
      Q_EMIT initialization_complete();
   });
}


QString ClientWrapper::get_info()
{
   fc::variant_object result = _bitshares_thread.async( [this](){ return _client->get_info(); }).wait();
   return QString::fromStdString(fc::json::to_pretty_string( result ));
}


void ClientWrapper::set_data_dir(QString data_dir)
{
   QSettings ("BitShares", BTS_BLOCKCHAIN_NAME).setValue("data_dir", data_dir);
}

void ClientWrapper::confirm_and_set_approval(QString delegate_name, bool approve)
{
   auto account = get_client()->blockchain_get_account(delegate_name.toStdString());
   if( account.valid() && account->is_delegate() )
   {
      if( QMessageBox::question(nullptr,
                                tr("Set Delegate Approval"),
                                tr("Would you like to update approval rating of Delegate %1 to %2?")
                                .arg(delegate_name)
                                .arg(approve?"Approve":"Disapprove")
                                )
          == QMessageBox::Yes )
         get_client()->wallet_account_set_approval(delegate_name.toStdString(), approve);
   }
   else
      QMessageBox::warning(nullptr, tr("Invalid Account"), tr("Account %1 is not a delegate, so its approval cannot be set.").arg(delegate_name));
}

QString ClientWrapper::create_voter_account()
{
   try {
      string name = "voter-" + string(fc::digest(fc::time_point::now())).substr(0, 8) + ".booth.follow-my-vote";
      qDebug() << "Creating voter account" << name.c_str();
      _client->wallet_account_create(name);
      return QString::fromStdString(name);
   } catch (fc::exception e) {
      qDebug("%s", e.to_detail_string().c_str());
      Q_EMIT error(e.to_detail_string().c_str());
   }
   return "";
}

void ClientWrapper::get_verification_request_status(QString account_name, QStringList verifiers, QJSValue callback)
{
   qDebug() << "Checking verification request status.";
   bts::vote::get_verification_message poll_message;
   auto account = _client->wallet_get_account(account_name.toStdString());
   poll_message.owner = account.active_address();
   poll_message.owner_address_signature = _client->wallet_sign_hash(account.name, fc::digest(poll_message.owner));

   _bitshares_thread.async([this, account_name, verifiers, poll_message, callback]() {
      auto key = lookup_public_key(verifiers);
      bts::mail::message mail = bts::mail::message(poll_message).encrypt(fc::ecc::private_key::generate(),
                                                                         key);
      mail.recipient = key;

      try {
         auto client = get_rpc_client(verifiers[0]);
         auto response = client->verifier_public_api(mail);
         process_verifier_response(response, account_name, callback);
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

void ClientWrapper::begin_verification(QObject* window, QString account_name, QStringList verifiers, QJSValue callback)
{
   if( verifiers.size() == 0 )
   {
      Q_EMIT error("No verifiers were specified.");
      return;
   }

   bts::vote::identity_verification_request* request = new bts::vote::identity_verification_request;
   request->owner = _client->wallet_get_account(account_name.toStdString()).active_key();
   request->properties.emplace_back(bts::vote::identity_property::generate("First Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Middle Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Last Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Date of Birth"));
   request->properties.emplace_back(bts::vote::identity_property::generate("ID Number"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Address Line 1"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Address Line 2"));
   request->properties.emplace_back(bts::vote::identity_property::generate("City"));
   request->properties.emplace_back(bts::vote::identity_property::generate("State"));
   request->properties.emplace_back(bts::vote::identity_property::generate("9-Digit ZIP"));
   request->owner_photo = window->property("userPhoto").toUrl().toLocalFile().toStdString();
   request->id_front_photo = window->property("idFrontPhoto").toUrl().toLocalFile().toStdString();
   request->id_back_photo = window->property("idBackPhoto").toUrl().toLocalFile().toStdString();
   request->voter_reg_photo = window->property("voterRegistrationPhoto").toUrl().toLocalFile().toStdString();

   //Punt this whole procedure out to the bitshares thread. We don't want to block the GUI thread.
   _bitshares_thread.async([this, account_name, verifiers, callback, request]() mutable {
      //Set photos
      QFile file(request->owner_photo.c_str());
      file.open(QIODevice::ReadOnly);
      request->owner_photo = file.readAll().toBase64().data();
      file.close();
      file.setFileName(request->id_front_photo.c_str());
      file.open(QIODevice::ReadOnly);
      request->id_front_photo = file.readAll().toBase64().data();
      file.close();
      file.setFileName(request->id_back_photo.c_str());
      file.open(QIODevice::ReadOnly);
      request->id_back_photo = file.readAll().toBase64().data();
      file.close();
      file.setFileName(request->voter_reg_photo.c_str());
      file.open(QIODevice::ReadOnly);
      request->voter_reg_photo = file.readAll().toBase64().data();
      file.close();

      bts::vote::identity_verification_request_message request_message;
      //Efficiently move the request into the message, then delete it. We don't want to copy or leak this thing; it's huge.
      request_message.request = std::move(*request);
      delete request;
      request = nullptr;
      //Sign request
      request_message.signature = _client->wallet_sign_hash(account_name.toStdString(), request_message.request.digest());

      bts::blockchain::public_key_type verifier_key = lookup_public_key(verifiers);

      bts::mail::message ciphertext = bts::mail::message(request_message).encrypt(fc::ecc::private_key::generate(),
                                                                                  verifier_key);
      ciphertext.recipient = verifier_key;
      auto client = get_rpc_client(verifiers[0]);
      try {
         auto response = client->verifier_public_api(ciphertext);

         process_verifier_response(response, account_name, callback);
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

bts::blockchain::public_key_type ClientWrapper::lookup_public_key(QStringList verifiers)
{
   auto verifier_account = _client->blockchain_get_account(verifiers[0].toStdString());
   bts::blockchain::public_key_type verifier_key;
   if( verifier_account )
      verifier_key = verifier_account->active_key();
   else {
      Q_EMIT error(QString("Could not find verifier account %1.").arg(verifiers[0]));
      verifier_key = bts::blockchain::public_key_type("XTS6LNgKuUmEH18TxXWDEeqMtYYQBBXWfE1ZDdSx95jjCJvnwnoGy");
   }

   return verifier_key;
}

std::shared_ptr<bts::rpc::rpc_client> ClientWrapper::get_rpc_client(QString verifiers)
{
   auto client = std::make_shared<bts::rpc::rpc_client>();
   client->connect_to(fc::ip::endpoint(fc::ip::address("127.0.0.1"), 3000));
   client->login("bob", "bob");

   return client;
}

void ClientWrapper::process_verifier_response(bts::mail::message response, QString account_name, QJSValue callback)
{
   if( response.type == bts::vote::exception_message_type )
      qDebug() << fc::json::to_pretty_string(response.as<bts::vote::exception_message>()).c_str();
   else if( response.type == bts::mail::encrypted ) {
      auto privKey = bts::utilities::wif_to_key(_client->wallet_dump_private_key(account_name.toStdString()));
      if( privKey ) {
         response = response.as<bts::mail::encrypted_message>().decrypt(*privKey);
      }
   }

   if( response.type == bts::vote::verification_response_message_type ) {
      auto response_message = response.as<bts::vote::identity_verification_response_message>();
      qDebug() << fc::json::to_pretty_string(response_message).c_str();
      callback.call(QJSValueList() << fc::json::to_string(response_message).c_str());
   } else {
      qDebug() << fc::json::to_pretty_string(response).c_str();
      callback.call(QJSValueList() << fc::json::to_string(response).c_str());
   }
}
