#include "LightWallet.hpp"
#include "QtConversions.hpp"

#include <bts/utilities/words.hpp>

#include <QDebug>
#include <QSettings>

#define IN_THREAD m_walletThread.async([=] {
#define END_THREAD }, __FUNCTION__);

inline static QString normalize(const QString& key)
{
   return key.toUpper().remove(QRegExp("[^A-Z]"));
}

bool LightWallet::walletExists() const
{
   return fc::exists(m_walletPath);
}

QString LightWallet::accountName() const
{
   if( !m_wallet._data )
      return QString();
   return convert(m_wallet._data->user_account.name);
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

   qDebug() << "Creating wallet:" << password << m_brainKey << salt.c_str();
   qDebug() << "Wallet path:" << m_walletPath.generic_string().c_str();

   try {
      m_wallet.create(m_walletPath, convert(accountName), convert(password), convert(normalize(m_brainKey)), salt);
   } catch (fc::exception e) {
      qDebug() << "Exception when creating wallet:" << e.to_detail_string().c_str();
   }

   Q_EMIT walletExistsChanged(walletExists());
   Q_EMIT openChanged(isOpen());
   Q_EMIT unlockedChanged(isUnlocked());
   Q_EMIT accountNameChanged(accountName);
   QSettings().setValue(QStringLiteral("brainKey"), m_brainKey);
   END_THREAD
}

void LightWallet::openWallet()
{
   IN_THREAD
   try {
      m_wallet.open(m_walletPath);
   } catch (fc::exception e) {
      m_openError = convert(e.to_string());
      Q_EMIT errorOpening(m_openError);
   }

   Q_EMIT openChanged(isOpen());
   Q_EMIT accountNameChanged(accountName());
   qDebug() << "Opened wallet belonging to" << accountName();

   m_brainKey = QSettings().value(QStringLiteral("brainKey"), QString()).toString();
   if( !m_brainKey.isEmpty() )
      Q_EMIT brainKeyChanged(m_brainKey);
   END_THREAD
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
   } catch (fc::exception e) {
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
   Q_EMIT unlockedChanged(isUnlocked());
   END_THREAD
}

void LightWallet::registerAccount()
{
   IN_THREAD
   if( m_wallet.request_register_account() ) {
      Q_EMIT accountRegistered();
      Q_EMIT accountNameChanged(accountName());
   } else
      Q_EMIT errorRegistering(QStringLiteral("Server did not register account. Try again later."));
   END_THREAD
}

void LightWallet::clearBrainKey()
{
   QSettings().remove(QStringLiteral("brainKey"));
   m_brainKey.clear();
   Q_EMIT brainKeyChanged(m_brainKey);
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
