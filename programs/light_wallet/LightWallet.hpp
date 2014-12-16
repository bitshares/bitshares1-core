#pragma once

#include <QObject>
#include <QString>

#include <bts/light_wallet/light_wallet.hpp>

#include <fc/thread/thread.hpp>

class LightWallet : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
   Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
   Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY unlockedChanged)
   Q_PROPERTY(QString connectionError READ connectionError NOTIFY errorConnecting)
   Q_PROPERTY(QString openError READ openError NOTIFY errorOpening)
   Q_PROPERTY(QString unlockError READ unlockError NOTIFY errorUnlocking)

public:
   LightWallet()
      : m_walletThread("Wallet Implementation Thread")
   {}
   ~LightWallet(){}

   QString connectionError() const
   {
      return m_connectionError;
   }
   QString openError() const
   {
      return m_openError;
   }
   QString unlockError() const
   {
      return m_unlockError;
   }
   bool isConnected() const
   {
      return m_wallet.is_connected();
   }
   bool isOpen() const
   {
      return m_wallet.is_open();
   }
   bool isUnlocked() const
   {
      return m_wallet.is_unlocked();
   }

public Q_SLOTS:
   void connectToServer( QLatin1String host, uint16_t port = 0,
                         QLatin1String user = QLatin1String("any"),
                         QLatin1String password = QLatin1String("none") );
   void disconnectFromServer();

   void openWallet();
   void closeWallet();

   void unlockWallet(QString password);
   void lockWallet();

Q_SIGNALS:
   void errorConnecting(QString error);
   void errorOpening(QString error);
   void errorUnlocking(QString arg);
   void connectedChanged(bool connected);
   void openChanged(bool open);
   void unlockedChanged(bool unlocked);

private:
   fc::thread m_walletThread;
   bts::light_wallet::light_wallet m_wallet;
   QString m_connectionError;
   QString m_openError;
   QString m_unlockError;
};
