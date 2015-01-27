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
   m_walletThread.async([&] {
#define END_THREAD }, __FUNCTION__);
#define END_WAIT_THREAD \
      QEventLoopLocker _THREAD_LOCKER_(&_THREAD_WAIT_LOOP_); \
   END_THREAD \
   fc::yield(); \
   _THREAD_WAIT_LOOP_.exec();

inline static QString normalize(const QString& key)
{
   return key.toUpper().remove(QRegExp("[^A-Z]"));
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

void LightWallet::connectToServer(QString host, quint16 port, QString user, QString password)
{
   IN_THREAD
   qDebug() << "Connecting to" << host << ':' << port << "as" << user << ':' << password;
   try {
      m_wallet.connect(convert(host), convert(user), convert(password), port);
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
   if( walletExists() ) {
      qDebug() << "Ignoring request to create wallet when wallet already exists!";
      return;
   }

   IN_THREAD
   generateBrainKey();
   std::string salt(bts::blockchain::address(fc::ecc::private_key::generate().get_public_key()));
   salt.erase(0, strlen(BTS_ADDRESS_PREFIX));

   qDebug() << "Wallet path:" << m_walletPath.generic_string().c_str();

   try {
      m_wallet.create(m_walletPath, convert(accountName), convert(password), convert(normalize(m_brainKey)), salt);
   } catch (fc::exception e) {
      qDebug() << "Exception when creating wallet:" << e.to_detail_string().c_str();
   }

   Q_EMIT walletExistsChanged(walletExists());
   Q_EMIT openChanged(isOpen());
   Q_EMIT unlockedChanged(isUnlocked());
   updateAccount(m_wallet.account());
   fc::sha512 key = fc::sha512::hash(convert(password));
   auto plaintext = std::vector<char>(m_brainKey.toStdString().begin(), m_brainKey.toStdString().end());
   auto ciphertext = fc::aes_encrypt(key, plaintext);
   QSettings().setValue(QStringLiteral("brainKey"), QByteArray::fromRawData(ciphertext.data(), ciphertext.size()));

   END_THREAD
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

   updateAccount(m_wallet.account());
   qDebug() << "Opened wallet belonging to" << m_account->name();
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

   IN_THREAD
   try {
      m_wallet.unlock(convert(password));
      if( isUnlocked() )
      {
         qDebug() << "Unlocked wallet; active address is" << std::string(m_wallet.account().active_address()).c_str();

         if( QSettings().contains("brainKey") )
         {
            auto ciphertext = QSettings().value(QStringLiteral("brainKey"), QString()).toByteArray();
            fc::sha512 key = fc::sha512::hash(convert(password));
            auto plaintext = fc::aes_decrypt(key, std::vector<char>(ciphertext.data(),
                                                                    ciphertext.data() + ciphertext.size()));
            m_brainKey = QString::fromLocal8Bit(plaintext.data());
            Q_EMIT brainKeyChanged(m_brainKey);
            qDebug() << m_brainKey;
         }
      }
   } catch (fc::exception e) {
      qDebug() << "Error during unlock:" << e.to_detail_string().c_str();
      m_unlockError = QStringLiteral("Incorrect password.");
      Q_EMIT errorUnlocking(m_unlockError);
   }

   Q_EMIT unlockedChanged(isUnlocked());
   END_THREAD
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

void LightWallet::registerAccount()
{
   IN_THREAD
   try {
      if( m_wallet.request_register_account() ) {
         for( int i = 0; i < 5; ++i ) {
            updateAccount(m_wallet.fetch_account());
            if( m_wallet.account().registration_date != fc::time_point_sec() )
               break;
            fc::usleep(fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC / 2));
         }
         if( m_wallet.account().registration_date == fc::time_point_sec() )
            Q_EMIT errorRegistering(QStringLiteral("Registration is taking longer than usual. Please try again later."));
      } else
         Q_EMIT errorRegistering(QStringLiteral("Server did not register account. Please try again later."));
   } catch( const bts::light_wallet::account_already_registered& e ) {
      //If light_wallet throws account_already_registered, it's because someone snagged the name already.
      qDebug() << "Account registered out from under us: " << e.to_detail_string().c_str();
      Q_EMIT errorRegistering(QStringLiteral("Oops! Someone just registered that name. Please pick another one."));
   } catch (const fc::exception& e) {
      const static QString message = QStringLiteral("Failed to register account: %1");
      if( e.get_log().empty() || e.get_log()[0].get_format().empty() )
         Q_EMIT errorRegistering(message.arg(e.what()));
      else
         Q_EMIT errorRegistering(message.arg(convert(e.get_log()[0].get_format())));
   }

   END_THREAD
}

void LightWallet::clearBrainKey()
{
   QSettings().remove(QStringLiteral("brainKey"));
   m_brainKey.clear();
   Q_EMIT brainKeyChanged(m_brainKey);
}

void LightWallet::sync()
{
   IN_THREAD
   if( !isConnected() ) return;

   m_wallet.sync_balance();
   m_wallet.sync_transactions();

   Q_EMIT synced();
   END_THREAD
}

void LightWallet::syncAllBalances()
{
   IN_THREAD
   QStringList currentAssets;
   if( account() )
      currentAssets = account()->availableAssets();
   m_wallet.sync_balance(true);
   if( account() && currentAssets.empty() && account()->availableAssets().empty() )
      Q_EMIT notification(tr("No new balances were found."));
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
   if( !m_account )
   {
      //Tricky threading. We're probably running in the bitshares thread, but we want Account to belong to the UI thread
      //so it can be a child of LightWallet (this).
      //Create it in this thread, then move it to the UI thread, then set its parent.
      m_account = new Account(&m_wallet, account);
      m_account->moveToThread(thread());
      m_account->setParent(this);
      connect(this, &LightWallet::synced, m_account, &Account::balancesChanged);
      Q_EMIT accountChanged(m_account);

      connect(m_account, &Account::nameChanged, [this](QString name) {
         if( m_wallet.account().registration_date == fc::time_point_sec() )
            m_wallet.account().name = convert(name);
         else
            m_account->setName(convert(m_wallet.account().name));
      });
   } else
      *m_account = account;
}
