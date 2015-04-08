#include "LightWallet.hpp"
#include "QtConversions.hpp"

#include <bts/utilities/words.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/digest.hpp>

#include <QDebug>
#include <QSettings>
#include <QEventLoop>

#include <iostream>

#define IN_THREAD \
   m_walletThread.async([=] {
#define END_THREAD }, __FUNCTION__);
#define IN_WAIT_THREAD \
   QEventLoop _THREAD_WAIT_LOOP_; \
   m_walletThread.async([&] { \
      QEventLoopLocker _THREAD_LOCKER_(&_THREAD_WAIT_LOOP_);
#define END_WAIT_THREAD \
   END_THREAD \
   _THREAD_WAIT_LOOP_.exec();

template<typename T>
class FutureGuardian : public QObject
{
   fc::future<T> m_future;
   const char* m_file;
   const char* m_function;
public:
   FutureGuardian(fc::future<T> future, const char* file, const char* function, QObject* parent)
      : QObject(parent),
        m_future(future),
        m_file(file),
        m_function(function)
   {
      m_future.on_complete([this](fc::exception_ptr e) {
         if( e )
            qDebug() << "Future from" << m_file << "function" << m_function
                     << "completed with error:" << e->to_detail_string().c_str();
         deleteLater();
      });
   }
   virtual ~FutureGuardian() {
      if( m_future.valid() && !m_future.ready() )
         m_future.cancel_and_wait(__FUNCTION__);
   }
};
template<typename T>
void GuardFuture(fc::future<T> f, const char* file, const char* function, QObject* owner) {
   new FutureGuardian<T>(f, file, function, owner);
}
#define GUARD_FUTURE(F) GuardFuture(F, __FILE__, __FUNCTION__, this)

inline static QString normalize(QString key)
{
   key = key.simplified();
   return key.toUpper();
}

LightWallet::LightWallet()
   : m_walletThread("Wallet Implementation Thread"),
     m_wallet([this](const std::string& key, const std::string& value)
{
   QSettings().setValue(QStringLiteral("Backend/%1").arg(key.c_str()), convert(value));
}, [this](const std::string& key)
{
   return convert(QSettings().value(QStringLiteral("Backend/%1").arg(key.c_str())).toString());
}, [this](const std::string& key) -> bool
{
   return QSettings().contains(QStringLiteral("Backend/%1").arg(key.c_str()));
})
{
   auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
   QDir(path).mkpath(".");
}

bool LightWallet::walletExists() const
{
   return QSettings().childGroups().contains(QStringLiteral("Backend"));
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

int LightWallet::getDigitsOfPrecision(QString assetSymbol)
{
   if( m_digitsOfPrecisionCache.contains(assetSymbol) )
      return m_digitsOfPrecisionCache[assetSymbol];
   int digits = log10l(BTS_BLOCKCHAIN_PRECISION);

   //Verify we're in the wallet thread
   if( !m_walletThread.is_current() )
      return m_walletThread.async([this, assetSymbol]() {
         return getDigitsOfPrecision(assetSymbol);
      }, __FUNCTION__).wait();

   if( assetSymbol.isEmpty() )
      return digits;

   if( auto rec = m_wallet.get_asset_record(convert(assetSymbol)) )
      digits = log10l(rec->precision);

   return m_digitsOfPrecisionCache[assetSymbol] = digits;
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

QStringList LightWallet::allAssets()
{
   QStringList assets;

   IN_WAIT_THREAD
   for( const std::string& symbol : m_wallet.all_asset_symbols() )
      assets.append(convert(symbol));
   END_WAIT_THREAD

   return assets;
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
      m_wallet.set_disconnect_callback([this](fc::exception_ptr) {
         Q_EMIT connectedChanged(false);
      });
      Q_EMIT connectedChanged(isConnected());
      Q_EMIT allAssetsChanged();
   } catch (fc::exception e) {
      if( e.get_log().size() )
         m_connectionError = convert(e.get_log().begin()->get_message()).replace("\n", " ");
      else
         m_connectionError = tr("Unknown error");
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

   try {
      m_wallet.create(convert(accountName), convert(password), convert(normalize(m_brainKey)));
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
   plaintext.push_back(char(0));
   auto ciphertext = fc::aes_encrypt(key, plaintext);
   QSettings().setValue(QStringLiteral("brainKey"), QByteArray::fromRawData(ciphertext.data(), ciphertext.size()));

   END_WAIT_THREAD
}

QStringList LightWallet::recoverableAccounts(QString brainKey)
{
   QStringList results;

   IN_WAIT_THREAD
   int n = 0;
   bts::blockchain::oaccount_record account;
   do {
      auto key = m_wallet.derive_private_key(convert(brainKey), n++);
      account = m_wallet.get_account_record(std::string(bts::blockchain::public_key_type(key.get_public_key())));
      if( account )
         results.append(convert(account->name));
   } while(account);
   END_WAIT_THREAD

   qDebug() << "Found recoverable accounts:" << results;
   return results;
}

bool LightWallet::recoverWallet(QString accountName, QString password, QString brainKey)
{
   bool success = false;

   IN_WAIT_THREAD
   if( m_wallet.is_open() )
      m_wallet.close();

   try {
      m_wallet.create(convert(accountName), convert(password), convert(normalize(brainKey)));
      auto account = m_wallet.fetch_account(convert(accountName));
      if( account.registration_date != fc::time_point_sec() )
      {
         m_wallet.sync_balance(true);
         m_wallet.sync_transactions(true);
         m_wallet.save();
         updateAccount(account);
         Q_EMIT synced();
         success = true;
      } else {
         Q_EMIT notification(tr("Couldn't recover account."));
      }
   } catch (bts::light_wallet::account_already_registered e) {
      Q_EMIT notification(tr("Could not claim %1. Is your recovery password correct?").arg(accountName));
      wdump((e.to_detail_string()));
   }

   Q_EMIT walletExistsChanged(walletExists());
   Q_EMIT openChanged(isOpen());
   Q_EMIT unlockedChanged(isUnlocked());
   END_WAIT_THREAD

   return success;
}

void LightWallet::openWallet()
{
   IN_WAIT_THREAD
   try {
      m_wallet.open();
   } catch (fc::exception e) {
      m_openError = convert(e.to_string());
      Q_EMIT errorOpening(m_openError);
   }

   Q_EMIT openChanged(isOpen());

   if( !isOpen() ) return;

   for( auto account : m_wallet.account_records() )
      updateAccount(*account);
   END_WAIT_THREAD

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
            m_brainKey = QString::fromLocal8Bit(plaintext.data(), plaintext.size());
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

void LightWallet::pollForRegistration(QString accountName)
{
   if( !m_walletThread.is_current() )
   {
      GUARD_FUTURE(m_walletThread.async([=] {
         pollForRegistration(accountName);
      }, __FUNCTION__));
      return;
   }

   std::string stdAccountName = convert(accountName);

   while( m_wallet.account(stdAccountName).registration_date == fc::time_point_sec() ) {
      updateAccount(m_wallet.fetch_account(stdAccountName));
      fc::usleep(fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC / 2));
   }
}

void LightWallet::registerAccount(QString accountName)
{
   IN_THREAD
   try {
      std::string stdAccountName = convert(accountName);
      if( m_wallet.request_register_account(stdAccountName) ) {
         pollForRegistration(accountName);
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

   bool dirty = m_wallet.sync_balance() | m_wallet.sync_transactions();
   if( dirty )
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

void LightWallet::syncAllTransactions()
{
   IN_THREAD
   if( m_wallet.sync_transactions(true) )
         Q_EMIT synced();
   END_THREAD
}

void LightWallet::generateBrainKey(uint8_t wordCount)
{
   FC_ASSERT( &fc::thread::current() == &m_walletThread );

   QString result;
   auto priv_key = fc::ecc::private_key::generate();
   auto secret = priv_key.get_secret();

   uint16_t* keys = (uint16_t*)&secret;
   for( uint32_t i = 0; i < wordCount; ++i )
   {
      // The private key secret is 32 bytes, which is good for 16 words. Every 16 words, we need to get a new secret
      // and begin iterating it again.
      static_assert((sizeof(secret._hash)/sizeof(uint16_t)) == 16, "Unexpected size for private key");
      if( i >= (sizeof(secret._hash)/sizeof(uint16_t)) )
      {
         wordCount -= i;
         i = 0;
         secret = fc::ecc::private_key::generate().get_secret();
      }
      result = result + word_list[keys[i]%word_list_size] + " ";
   }
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
      connect(newAccount, &Account::error, [this, newAccount] (QString error) {
         Q_EMIT notification(tr("Error from account %1: %2").arg(newAccount->name()).arg(error));
      });

      m_accounts.insert(accountName, QVariant::fromValue(newAccount));

      //Precache digits of precision for all held assets
      for( QString symbol : newAccount->availableAssets() )
         getDigitsOfPrecision(symbol);
   }
   Q_EMIT accountsChanged(m_accounts);
}
