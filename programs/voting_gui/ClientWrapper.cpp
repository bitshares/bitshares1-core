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
#include <QJsonObject>

#include <iostream>

using std::string;
using std::vector;
using bts::vote::ballot;
using bts::vote::contest;
using bts::vote::digest_type;

ClientWrapper::ClientWrapper(QObject *parent)
   : QObject(parent),
     _main_thread(&fc::thread::current()),
     _bitshares_thread("bitshares"),
     _settings("BitShares", BTS_BLOCKCHAIN_NAME)
{
   _block_interval_timer.setInterval(1000);
   connect(&_block_interval_timer, &QTimer::timeout, this, &ClientWrapper::state_changed);
   _block_interval_timer.start();
   _bitshares_thread.async([this]{
      create_secret();
   }, "create_secret");
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

void ClientWrapper::load_election()
{
   QFile json_file(":/res/ballots.json");
   json_file.open(QIODevice::ReadOnly);
   QByteArray json_dump;
   json_dump = json_file.readAll();
   json_file.close();
   vector<ballot> ballot_array = fc::json::from_string(string(json_dump.data(), json_dump.size())).as<vector<ballot>>();
   std::for_each(ballot_array.begin(), ballot_array.end(), [this](const ballot& bal) {
      _ballot_map[bal.id()] = bal;
   });
   json_file.setFileName(":/res/contests.json");
   json_file.open(QIODevice::ReadOnly);
   json_dump = json_file.readAll();
   json_file.close();
   vector<contest> contest_array = fc::json::from_string(string(json_dump.data(), json_dump.size())).as<vector<contest>>();
   std::for_each(contest_array.begin(), contest_array.end(), [this](const contest& con) {
      _contest_map[con.id()] = con;
   });
   qDebug() << "Loaded" << _contest_map.size() << "contests and" << _ballot_map.size() << "ballots.";
}

QString ClientWrapper::create_voter_account()
{
   //TODO: Make the caller listen to accountNameChanged rather than treating this call as synchronous
   QEventLoop waiter;
   connect(this, &ClientWrapper::accountNameChanged, &waiter, &QEventLoop::quit);
   _bitshares_thread.async([this]{
      try {
         string name = "voter-" + string(fc::digest(fc::time_point::now())).substr(0, 8) + ".booth.follow-my-vote";
         qDebug() << "Creating voter account" << name.c_str() << "with secret" << secret();
         auto key = fc::ecc::private_key::generate_from_seed(fc::digest(secret().toStdString()));
         _client->wallet_import_private_key(bts::utilities::key_to_wif(key), name, true, false);
         key = fc::ecc::private_key::generate_from_seed(fc::digest("voting key" + secret().toStdString()));
         m_voterAddress = QString::fromStdString(string(bts::blockchain::address(key.get_public_key())));
         Q_EMIT voter_address_changed(m_voterAddress);
         _client->wallet_import_private_key(bts::utilities::key_to_wif(key), "voting-key." + name, true, false);
         setAccountName(QString::fromStdString(name));
      } catch (fc::exception e) {
         qDebug("%s", e.to_detail_string().c_str());
         Q_EMIT error(e.to_detail_string().c_str());
      }
   }, __FUNCTION__);
   waiter.exec();
   return accountName();
}

QJsonObject ClientWrapper::get_voter_ballot()
{
   try {
      bts::vote::digest_type ballot_id = _client->get_wallet()->get_account(accountName().toStdString()).private_data
            .as<fc::variant_object>()["identity"]
            .as<bts::vote::signed_identity>().get_property("Ballot ID")->value
            .as<bts::vote::digest_type>();
      fc::mutable_variant_object ballot_with_id = fc::variant(_ballot_map[ballot_id]).as<fc::mutable_variant_object>();
      ballot_with_id["id"] = ballot_id;
      string json_ballot = fc::json::to_string(ballot_with_id);

      return QJsonDocument::fromJson(QByteArray(json_ballot.data(), json_ballot.size())).object();
   } catch (fc::exception& e) {
      Q_EMIT error(fc::json::to_string(e).c_str());
      return QJsonObject();
   }
}

QJsonArray ClientWrapper::get_voter_contests()
{
   bts::vote::digest_type ballot_id(get_voter_ballot()["id"].toString().toStdString());
   ballot bal = _ballot_map[ballot_id];

   QJsonArray contests;
   std::for_each(bal.contests.begin(), bal.contests.end(),
                 [this, &contests](const digest_type& id) {
      contests.append(get_contest_by_id(id));
   });
   return contests;
}

QJsonObject ClientWrapper::get_contest_by_id(QString contest_id)
{
   return get_contest_by_id(digest_type(contest_id.toStdString()));
}

QJsonObject ClientWrapper::get_contest_by_id(bts::vote::digest_type contest_id)
{
   if( _contest_map.find(contest_id) != _contest_map.end() ) {
      fc::mutable_variant_object contest_with_id = fc::variant(_contest_map[contest_id]).as<fc::mutable_variant_object>();
      contest_with_id["id"] = contest_id;
      string contest = fc::json::to_string(contest_with_id);
      return QJsonDocument::fromJson(QByteArray(contest.data(), contest.size())).object();
   }
   return QJsonObject();
}

void ClientWrapper::get_verification_request_status(QStringList verifiers, QJSValue callback)
{
   qDebug() << "Checking verification request status.";
   bts::vote::get_verification_message poll_message;
   auto account = _client->wallet_get_account(accountName().toStdString());
   poll_message.owner = account.active_address();
   poll_message.owner_address_signature = _client->wallet_sign_hash(account.name, fc::digest(poll_message.owner));

   _bitshares_thread.async([this, verifiers, poll_message, callback]() {
      auto key = lookup_public_key(verifiers[0]);
      bts::mail::message mail = bts::mail::message(poll_message).encrypt(fc::ecc::private_key::generate(), key);
      mail.recipient = key;

      try {
         auto client = get_rpc_client(verifiers[0]);
         auto response = client->verifier_public_api(mail);
         QMetaObject::invokeMethod(this, "process_verifier_response", Qt::QueuedConnection,
                                   Q_ARG(bts::mail::message, response), Q_ARG(QJSValue, callback));
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

void ClientWrapper::process_accepted_identity(QString identity, QJSValue callback)
{
   _bitshares_thread.async([this, identity, callback]() mutable {
      try {
         //Parse identity back to struct
         bts::vote::identity new_id = fc::json::from_string(identity.toStdString()).as<bts::vote::identity>();

         //Fetch identity from wallet
         fc::mutable_variant_object data = _client->get_wallet()->get_account(accountName().toStdString())
               .private_data.as<fc::mutable_variant_object>();
         if( data.find("identity") != data.end() )
         {
            bts::vote::signed_identity old_id = data["identity"].as<bts::vote::signed_identity>();
            FC_ASSERT(new_id.owner == old_id.owner, "Verifier identity has unexpected owner.");

            old_id.properties = merge_identities(old_id, new_id);
            old_id.sign_by_owner(_client->get_wallet()->get_active_private_key(accountName().toStdString()));
            data["identity"] = old_id;
            new_id = old_id;
         } else {
            FC_ASSERT(new_id.owner == _client->get_wallet()->get_account(accountName().toStdString()).owner_address(),
                      "Verifier identity has unexpected owner.");

            bts::vote::signed_identity signed_id;
            (bts::vote::identity&)signed_id = new_id;
            signed_id.sign_by_owner(_client->get_wallet()->get_active_private_key(accountName().toStdString()));
            data["identity"] = signed_id;
         }

         _client->get_wallet()->update_account_private_data(accountName().toStdString(), data);

         if( auto ballot_id = new_id.get_property("Ballot ID") )
         {
            qDebug() << "Verified ID:" << fc::json::to_pretty_string(new_id).c_str() << "\nBallot ID:" << fc::json::to_pretty_string(*ballot_id).c_str();
            QMetaObject::invokeMethod(this, "notify_acceptances",
                                      Q_ARG(int, ballot_id->verifier_signatures.size()), Q_ARG(QJSValue, callback));
         }
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

void ClientWrapper::begin_verification(QObject* window, QStringList verifiers, QJSValue callback)
{
   if( verifiers.size() == 0 )
   {
      Q_EMIT error("No verifiers were specified.");
      return;
   }

   bts::vote::identity_verification_request* request = new bts::vote::identity_verification_request;
   request->owner = _client->wallet_get_account(accountName().toStdString()).active_key();
   request->properties.emplace_back(bts::vote::identity_property::generate("First Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Middle Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Last Name"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Name Suffix"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Date of Birth"));
   request->properties.emplace_back(bts::vote::identity_property::generate("ID Number"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Address Line 1"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Address Line 2"));
   request->properties.emplace_back(bts::vote::identity_property::generate("City"));
   request->properties.emplace_back(bts::vote::identity_property::generate("State"));
   request->properties.emplace_back(bts::vote::identity_property::generate("ZIP"));
   request->properties.emplace_back(bts::vote::identity_property::generate("Ballot ID"));
   request->owner_photo = window->property("userPhoto").toUrl().toLocalFile().toStdString();
   request->id_front_photo = window->property("idFrontPhoto").toUrl().toLocalFile().toStdString();
   request->id_back_photo = window->property("idBackPhoto").toUrl().toLocalFile().toStdString();

   //Punt this whole procedure out to the bitshares thread. We don't want to block the GUI thread.
   _bitshares_thread.async([this, verifiers, callback, request]() mutable {
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

      bts::vote::identity_verification_request_message request_message;
      //Efficiently move the request into the message, then delete it. We don't want to copy or leak this thing; it's huge.
      request_message.request = std::move(*request);
      delete request;
      request = nullptr;

      //Store ID in wallet
      try {
         fc::mutable_variant_object data;
         try {
            data = _client->get_wallet()->get_account(accountName().toStdString())
                  .private_data.as<fc::mutable_variant_object>();
         } catch (fc::bad_cast_exception){/* If private data isn't a JSON object, it should be. Overwrite it. */}
         bts::vote::signed_identity my_id;
         (bts::vote::identity&)my_id = request_message.request;
         my_id.sign_by_owner(_client->get_wallet()->get_active_private_key(accountName().toStdString()));
         data["identity"] = std::move(my_id);
         _client->get_wallet()->update_account_private_data(accountName().toStdString(), data);
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }

      //Sign request
      request_message.signature = _client->wallet_sign_hash(accountName().toStdString(),
                                                            request_message.request.digest());

      bts::blockchain::public_key_type verifier_key = lookup_public_key(verifiers[0]);

      bts::mail::message ciphertext = bts::mail::message(request_message).encrypt(fc::ecc::private_key::generate(),
                                                                                  verifier_key);
      ciphertext.recipient = verifier_key;
      auto client = get_rpc_client(verifiers[0]);
      try {
         auto response = client->verifier_public_api(ciphertext);

         QMetaObject::invokeMethod(this, "process_verifier_response", Qt::QueuedConnection,
                                   Q_ARG(bts::mail::message, response), Q_ARG(QJSValue, callback));
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

void ClientWrapper::begin_registration(QStringList registrars)
{
   if( registrars.empty() )
   {
      Q_EMIT error("No registrars provided when asked to begin registration!");
      return;
   }

   _bitshares_thread.async([this, registrars]() mutable {
      try {
         std::shared_ptr<bts::rpc::rpc_client> client = get_rpc_client(registrars[0]);
         bts::wallet::wallet_account_record account = _client->get_wallet()->get_account(accountName().toStdString());
         auto identity = account.private_data.as<fc::variant_object>()["identity"].as<bts::vote::signed_identity>();
         auto ballot_id = identity.get_property("Ballot ID");
         FC_ASSERT(ballot_id, "Cannot begin registration because ballot ID has not been verified yet.");
         auto signature = _client->get_wallet()->sign_hash("voting-key." + account.name, fc::digest(*ballot_id));

         try {
            auto registrar_signature = client->registrar_demo_registration(*ballot_id, identity.owner, signature);
            auto data = account.private_data.as<fc::mutable_variant_object>();
            data["registrar_signatures"] = std::vector<bts::vote::expiring_signature>({registrar_signature});
            _client->get_wallet()->update_account_private_data(account.name, data);
            Q_EMIT registered();
         } catch (fc::exception& e) {
            qDebug() << e.to_detail_string().c_str();
            Q_EMIT error(fc::json::to_string(e).c_str());
         }
      } catch (fc::exception& e) {
         qDebug() << e.to_detail_string().c_str();
         Q_EMIT error(fc::json::to_string(e).c_str());
      }
   }, __FUNCTION__);
}

void ClientWrapper::submit_decisions(QString json_decisions)
{
   auto future = _bitshares_thread.async([this, json_decisions]() mutable {
      auto decisions = fc::json::from_string(json_decisions.toStdString()).as<vector<bts::vote::voter_decision>>();
      auto authorizations = _client->get_wallet()->get_account(accountName().toStdString()).private_data
            .as<fc::variant_object>()["registrar_signatures"]
            .as<vector<bts::vote::expiring_signature>>();
      auto voter_key = _client->get_wallet()->get_active_private_key("voting-key." + accountName().toStdString());
      vector<fc::variants> signed_decisions;
      signed_decisions.reserve(decisions.size());

      for( bts::vote::voter_decision& decision : decisions )
      {
         decision.registrar_authorizations = authorizations;
         signed_decisions.emplace_back(fc::variants(1, fc::variant(bts::vote::signed_voter_decision(decision, voter_key))));
      }

      auto client = get_rpc_client("ballot_box");
      client->batch("ballot_submit_decision", signed_decisions);

      Q_EMIT decisions_submitted();
   }, __FUNCTION__);
   future.on_complete([this](fc::exception_ptr e) {
      if( e )
         Q_EMIT error(fc::exception(*e).to_detail_string().c_str());
   });
}

bts::blockchain::public_key_type ClientWrapper::lookup_public_key(QString account_name)
{
   auto account = _client->blockchain_get_account(account_name.toStdString());
   bts::blockchain::public_key_type account_key;
   if( account )
      account_key = account->active_key();
   else {
      if( account_name == "verifier" )
         account_key = bts::blockchain::public_key_type("XTS6LNgKuUmEH18TxXWDEeqMtYYQBBXWfE1ZDdSx95jjCJvnwnoGy");
      else if( account_name == "registrar" )
         account_key = bts::blockchain::public_key_type("XTS7AfNMa1ZUdv7EfhnJCj11km5NM8SRpyDCNcoMQfrEHwpLwuJfW");
      else
         Q_EMIT error(QString("Could not find account %1.").arg(account_name));
   }

   return account_key;
}

std::shared_ptr<bts::rpc::rpc_client> ClientWrapper::get_rpc_client(QString account)
{
   auto client = std::make_shared<bts::rpc::rpc_client>();
   client->connect_to(fc::ip::endpoint(fc::ip::address("69.90.132.209"), 3000));
   client->login("bob", "bob");

   return client;
}

void ClientWrapper::process_verifier_response(bts::mail::message response, QJSValue callback)
{
   if( response.type == bts::vote::exception_message_type )
      qDebug() << fc::json::to_pretty_string(response.as<bts::vote::exception_message>()).c_str();
   else if( response.type == bts::mail::encrypted ) {
      auto privKey = bts::utilities::wif_to_key(_client->wallet_dump_private_key(accountName().toStdString()));
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

void ClientWrapper::notify_acceptances(int acceptance_count, QJSValue callback)
{
   callback.call(QJSValueList() << acceptance_count);
}

std::vector<bts::vote::signed_identity_property> ClientWrapper::merge_identities(const bts::vote::signed_identity& old_id,
                                                                                 bts::vote::identity new_id)
{
   auto working_properties = old_id.properties;

   //Clean invalid signatures from old identity
   purge_invalid_signatures(working_properties, old_id.owner);

   for( const bts::vote::signed_identity_property& new_property : new_id.properties )
   {
      //Find iterator to property with same name, salt and value
      auto itr = std::find_if(working_properties.begin(), working_properties.end(),
                              [&new_property](const bts::vote::signed_identity_property& old_property) -> bool {
         return (bts::vote::identity_property&)old_property == (bts::vote::identity_property&)new_property;
      });
      if( itr != working_properties.end() )
      {
         //This property is already in old identity. Merge signatures.
         bts::vote::signed_identity_property& old_property = *itr;
         //If old_property is an empty placeholder, just overwrite it.
         if( old_property.value.is_null() && !new_property.value.is_null() )
         {
            old_property = new_property;
            continue;
         }

         for( const bts::vote::expiring_signature& new_signature : new_property.verifier_signatures )
         {
            fc::ecc::public_key new_signer = new_signature.signer(new_property.id(new_id.owner));
            if( new_signer == fc::ecc::public_key() )
               //Invalid signature. Skip it.
               continue;
            //Find iterator to signature with same signer
            auto itr = std::find_if(old_property.verifier_signatures.begin(), old_property.verifier_signatures.end(),
                                    [new_signer, old_property, old_id](const bts::vote::expiring_signature& old_signature)
                                    -> bool {
               return old_signature.signer(old_property.id(old_id.owner)) == new_signer;
            });
            if( itr != old_property.verifier_signatures.end() )
            {
               //This signer has signed this property before. Keep the signature which expires later.
               if( new_signature.valid_until > itr->valid_until )
                  *itr = new_signature;
            } else {
               //This signer has not signed this property. Insert the new signature.
               old_property.verifier_signatures.push_back(new_signature);
            }
         }
      } else {
         //This property is not in old identity yet. Add it.
         working_properties.push_back(new_property);
      }
   }

   return working_properties;
}

void ClientWrapper::purge_invalid_signatures(std::vector<bts::vote::signed_identity_property>& properties,
                                             bts::blockchain::address owner)
{
   for( bts::vote::signed_identity_property& property : properties)
      property.verifier_signatures.erase(std::remove_if(property.verifier_signatures.begin(),
                                                        property.verifier_signatures.end(),
                                                        [&property, &owner](
                                                           const bts::vote::expiring_signature& signature
                                                        ) -> bool {
         return signature.signer(property.id(owner)) == fc::ecc::public_key();
      }), property.verifier_signatures.end());
}

void ClientWrapper::create_secret()
{
   QFile dictionary(":/res/words");
   int wordCount = 0;

   if( dictionary.open(QIODevice::ReadOnly | QIODevice::Text) )
   {
      QTextStream dictionaryStream(&dictionary);
      while( !dictionaryStream.atEnd() )
      {
         dictionaryStream.readLine();
         ++wordCount;
      }
      qDebug() << "Creating secret from" << wordCount << "word dictionary.";
   } else {
      qDebug() << "Cannot open dictionary:" << dictionary.errorString() << "-- falling back to private key.";
   }

   if( wordCount > 0 ) {
      m_secret.clear();
      for( int i = 0; i < 5; ++i )
      {
         int randomLine = *((int*)fc::ecc::private_key::generate().get_secret().data()) % wordCount;
         dictionary.seek(0);
         QByteArray word;
         for( int l = 0; l < randomLine; ++l ) word = dictionary.readLine().trimmed();
         if( word.length() > 2 )
         {
            if( i > 0 ) m_secret += " ";
            m_secret += word;
         } else
            --i;
      }
   } else m_secret = fc::variant(fc::ecc::private_key::generate()).as_string().c_str();

   dictionary.close();

   Q_EMIT secretChanged(m_secret);
}
