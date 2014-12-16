#include "LightWallet.hpp"
#include "QtConversions.hpp"

#include <QStandardPaths>

#define IN_THREAD m_walletThread.async([&] {
#define END_THREAD }, __FUNCTION__);

void LightWallet::connectToServer(QLatin1String host, uint16_t port, QLatin1String user, QLatin1String password)
{
   IN_THREAD
   try {
      m_wallet.connect(host.latin1(), user.latin1(), password.latin1(), port);
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

void LightWallet::openWallet()
{
   IN_THREAD
   try {
      fc::path wallet_path = fc::path(QStandardPaths::writableLocation(QStandardPaths::DataLocation).toStdWString())
            / "wallet_data.json";
      m_wallet.open(wallet_path);
   } catch (fc::exception e) {
      m_openError = convert(e.to_string());
      Q_EMIT errorOpening(m_openError);
   }

   Q_EMIT openChanged(isOpen());
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
