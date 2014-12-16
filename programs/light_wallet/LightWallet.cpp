#include "LightWallet.hpp"
#include "QtConversions.hpp"

#include <bts/utilities/words.hpp>

#include <QStandardPaths>
#include <QDebug>
#include <QSettings>

#define IN_THREAD m_walletThread.async([&] {
#define END_THREAD }, __FUNCTION__);

const static fc::path wallet_path = fc::path(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                                             .toStdWString())
      / "wallet_data.json";

inline static QString normalize(const QString& key)
{
   return key.toUpper().remove(QRegExp("[^A-Z]"));
}

bool LightWallet::walletExists() const
{
   return fc::exists(wallet_path);
}

void LightWallet::connectToServer(QString host, uint16_t port, QString user, QString password)
{
   IN_THREAD
   try {
      m_wallet.connect(convert(host), convert(user), convert(password), port);
   } catch (fc::exception e) {
      m_connectionError = convert(e.to_string());
      Q_EMIT errorConnecting(m_connectionError);
   }

   Q_EMIT connectedChanged(isConnected());
   END_THREAD
}

void LightWallet::disconnectFromServer()
{
   IN_THREAD
   m_wallet.disconnect();
   Q_EMIT connectedChanged(isConnected());
   END_THREAD
}

void LightWallet::createWallet(QString password)
{
   if( walletExists() ) {
      qDebug() << "Ignoring request to create wallet when wallet already exists!";
      return;
   }

   IN_THREAD
   generateBrainKey();
   std::string salt(bts::blockchain::address(fc::ecc::private_key::generate().get_public_key()));
   salt.erase(0, strlen(BTS_ADDRESS_PREFIX));

   m_wallet.create(wallet_path, convert(password), convert(normalize(m_brainKey)), salt);
   Q_EMIT walletExistsChanged(walletExists());
   Q_EMIT openChanged(isOpen());
   Q_EMIT unlockedChanged(isUnlocked());
   QSettings().setValue(QStringLiteral("brainKey"), m_brainKey);
   END_THREAD
}

void LightWallet::openWallet()
{
   IN_THREAD
   try {
      m_wallet.open(wallet_path);
   } catch (fc::exception e) {
      m_openError = convert(e.to_string());
      Q_EMIT errorOpening(m_openError);
   }

   Q_EMIT openChanged(isOpen());

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
   IN_THREAD
   if( !isOpen() )
   {
      openWallet();
   }

   try {
      m_wallet.unlock(convert(password));
   } catch (fc::exception) {
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
