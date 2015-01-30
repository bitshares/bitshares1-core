#include "LightWallet.hpp"
#include "QtConversions.hpp"

#include <bts/utilities/words.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/digest.hpp>

#include <QDebug>
#include <QSettings>
#include <QEventLoop>

#define IN_THREAD \
   m_walletThread.async([=] {
#define IN_WAIT_THREAD \
   QEventLoop _THREAD_WAIT_LOOP_; \
   m_walletThread.async([&] { \
      QEventLoopLocker _THREAD_LOCKER_(&_THREAD_WAIT_LOOP_);
#define END_THREAD }, __FUNCTION__);
#define END_WAIT_THREAD \
   END_THREAD \
   _THREAD_WAIT_LOOP_.exec();

inline static QString normalize(QString key)
{
   key = key.simplified();
   return key.toUpper().remove(QRegExp("[^A-Z] "));
}

bool LightWallet::walletExists() const
{
   return fc::exists(m_walletPath);
}

Balance* LightWallet::getFee(QString assetSymbol)
{
   // QML takes ownership of these objects, and will delete them.
   Balance* fee = new Balance(assetSymbol, -1);

   IN_WAIT_THREAD
      try {
         auto rawFee = m_wallet.get_fee(convert(assetSymbol));
         auto feeAsset = m_wallet.get_asset_record(rawFee.asset_id);
         fee->setProperty("amount", double(rawFee.amount) / feeAsset->precision);
         fee->setProperty("symbol", convert(feeAsset->symbol));
      } catch (const fc::exception& e) {
         qDebug() << "Unable to get fee for" << assetSymbol << "because" << e.to_detail_string().c_str();
      }
   END_WAIT_THREAD

   return fee;
}

bool LightWallet::accountExists(QString name)
{
   bool exists;

   IN_WAIT_THREAD
   auto account = m_wallet.get_account_record(convert(name));
   exists = account.valid() && account->name == convert(name);
   END_WAIT_THREAD

   return exists;
}

bool LightWallet::verifyBrainKey(QString key) const
{
   return !m_brainKey.isEmpty() && normalize(key) == normalize(m_brainKey);
}

void LightWallet::connectToServer(QString host, quint16 port, QString serverKey, QString user, QString password)
{
   IN_THREAD
   qDebug() << "Connecting to" << host << ':' << port << "as" << user << ':' << password;
   try {
      if( serverKey.isEmpty() )
         m_wallet.connect(convert(host), convert(user), convert(password), port);
      else
         m_wallet.connect(convert(host), convert(user), convert(password), port,
                          bts::blockchain::public_key_type(convert(serverKey)));
      Q_EMIT connectedChanged(isConnected());
   } catch (fc::exception e) {
      m_connectionError = convert(e.get_log().begin()->get_message()).replace("\n", " ");
      Q_EMIT errorConnecting(m_connectionError);
   }
   END_THREAD
}

void LightWallet::disconnectFromServer()
{
   IN_THREAD
   m_wallet.disconnect();
   Q_EMIT connectedChanged(isConnected());
   END_THREAD
}

void LightWallet::createWallet(QString accountName, QString password)
{
   IN_WAIT_THREAD
   if( m_wallet.is_open() )
      m_wallet.close();
   generateBrainKey();

   qDebug() << "Wallet path:" << m_walletPath.generic_string().c_str();

   try {
      m_wallet.create(m_walletPath, convert(accountName), convert(password), convert(normalize(m_brainKey)));
   } catch (fc::exception e) {
      qDebug() << "Exception when creating wallet:" << e.to_detail_string().c_str();
   }

   qDebug() << "Created wallet.";

   Q_EMIT walletExistsChanged(walletExists());
   Q_EMIT openChanged(isOpen());
   Q_EMIT unlockedChanged(isUnlocked());
   updateAccount(m_wallet.account(convert(accountName)));
   fc::sha512 key = fc::sha512::hash(convert(password));
   auto brainKey = convert(m_brainKey);
   auto plaintext = std::vector<char>(brainKey.begin(), brainKey.end());
   auto ciphertext = fc::aes_encrypt(key, plaintext);
   QSettings().setValue(QStringLiteral("brainKey"), QByteArray::fromRawData(ciphertext.data(), ciphertext.size()));
   END_WAIT_THREAD
}

void LightWallet::openWallet()
{
   //Opening the wallet should be fast, it's just a local file and we're not doing much crypto. Let it be blocking.
   try {
      m_wallet.open(m_walletPath);
   } catch (fc::exception e) {
      m_openError = convert(e.to_string());
      Q_EMIT errorOpening(m_openError);
   }

   Q_EMIT openChanged(isOpen());

   if( !isOpen() ) return;

   for( auto account : m_wallet.account_records() )
      updateAccount(*account);

   qDebug() << "Opened wallet with" << m_accounts.size() << "accounts";
}

void LightWallet::closeWallet()
{
   IN_THREAD
   m_wallet.save();
   m_wallet.close();
   Q_EMIT openChanged(isOpen());
   END_THREAD
}

void LightWallet::unlockWallet(QString password)
{
   if( !isOpen() )
      openWallet();

   IN_WAIT_THREAD
   try {
      m_wallet.unlock(convert(password));
      if( isUnlocked() )
      {
         if( QSettings().contains("brainKey") )
         {
            auto ciphertext = QSettings().value(QStringLiteral("brainKey"), QString()).toByteArray();
            fc::sha512 key = fc::sha512::hash(convert(password));
            auto plaintext = fc::aes_decrypt(key, std::vector<char>(ciphertext.data(),
                                                                    ciphertext.data() + ciphertext.size()));
            m_brainKey = QString::fromLocal8Bit(plaintext.data());
            Q_EMIT brainKeyChanged(m_brainKey);
         }
      }
   } catch (fc::exception e) {
      qDebug() << "Error during unlock:" << e.to_detail_string().c_str();
      m_unlockError = QStringLiteral("Incorrect password.");
      Q_EMIT errorUnlocking(m_unlockError);
   }

   Q_EMIT unlockedChanged(isUnlocked());
   END_WAIT_THREAD
}

void LightWallet::lockWallet()
{
   IN_THREAD
   m_wallet.lock();
   qDebug() << "Locked wallet.";
   Q_EMIT unlockedChanged(isUnlocked());

   if( !m_brainKey.isEmpty() )
   {
      m_brainKey.clear();
      Q_EMIT brainKeyChanged(m_brainKey);
   }
   END_THREAD
}

void LightWallet::registerAccount(QString accountName)
{
   IN_THREAD
   try {
      std::string stdAccountName = convert(accountName);
      if( m_wallet.request_register_account(stdAccountName) ) {
         for( int i = 0; i < 5; ++i ) {
            updateAccount(m_wallet.fetch_account(stdAccountName));
            if( m_wallet.account(stdAccountName).registration_date != fc::time_point_sec() )
               break;
            fc::usleep(fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC / 2));
         }
         if( m_wallet.account(stdAccountName).registration_date == fc::time_point_sec() )
            Q_EMIT errorRegistering(tr("Registration is taking longer than usual. Please try again later."));
      } else
         Q_EMIT errorRegistering(tr("Server could not register account. Please try again later."));
   } catch( const bts::light_wallet::account_already_registered& e ) {
      //If light_wallet throws account_already_registered, it's because someone snagged the name already.
      qDebug() << "Account registered out from under us: " << e.to_detail_string().c_str();
      Q_EMIT errorRegistering(QStringLiteral("Oops! Someone just registered that name. Please pick another one."));
   } catch (const fc::exception& e) {
      const static QString message = tr("Failed to register account: %1");
      if( e.get_log().empty() || e.get_log()[0].get_format().empty() )
         Q_EMIT errorRegistering(message.arg(e.what()));
      else
         Q_EMIT errorRegistering(message.arg(convert(e.get_log()[0].get_format())));
   }

   END_THREAD
}

void LightWallet::clearBrainKey()
{
   fc::sha512 key;
   m_brainKey.replace(QRegExp("."), " ");
   auto brainKey = convert(m_brainKey);
   auto plaintext = std::vector<char>(brainKey.begin(), brainKey.end());
   auto ciphertext = fc::aes_encrypt(key, plaintext);
   QSettings().setValue(QStringLiteral("brainKey"), QByteArray::fromRawData(ciphertext.data(), ciphertext.size()));
   QSettings().remove(QStringLiteral("brainKey"));
   m_brainKey.clear();
   Q_EMIT brainKeyChanged(m_brainKey);
}

void LightWallet::sync()
{
   IN_THREAD
   if( !isConnected() ) return;

   if( m_wallet.sync_balance() || m_wallet.sync_transactions() )
      Q_EMIT synced();

   END_THREAD
}

void LightWallet::syncAllBalances()
{
   IN_THREAD
   if( m_wallet.sync_balance(true) )
         Q_EMIT synced();
   END_THREAD
}

void LightWallet::generateBrainKey()
{
   FC_ASSERT( &fc::thread::current() == &m_walletThread );

   QString result;
   auto priv_key = fc::ecc::private_key::generate();
   auto secret = priv_key.get_secret();

   uint16_t* keys = (uint16_t*)&secret;
   for( uint32_t i = 0; i < 9; ++i )
      result = result + word_list[keys[i]%word_list_size] + " ";
   result.chop(1);

   m_brainKey = result;
   Q_EMIT brainKeyChanged(m_brainKey);
}

void LightWallet::updateAccount(const bts::blockchain::account_record& account)
{
   QString accountName = convert(account.name);
   if( m_accounts.contains(accountName) )
      *qvariant_cast<Account*>(m_accounts[accountName]) = account;
   else
   {
      //Tricky threading. We're probably running in the bitshares thread, but we want Account to belong to the UI thread
      //so it can be a child of LightWallet (this).
      //Create it in this thread, then move it to the UI thread, then set its parent.
      Account* newAccount;
      newAccount = new Account(&m_wallet, account);
      newAccount->moveToThread(thread());
      newAccount->setParent(this);
      connect(this, &LightWallet::synced, newAccount, &Account::balancesChanged);

      m_accounts.insert(accountName, QVariant::fromValue(newAccount));
   }
   Q_EMIT accountsChanged(m_accounts);
}
