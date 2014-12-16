#pragma once

#include <QObject>
#include <QString>

#include <bts/light_wallet/light_wallet.hpp>

#include <fc/thread/thread.hpp>

class LightWallet : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool walletExists READ walletExists NOTIFY walletExistsChanged STORED false)
   Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
   Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
   Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY unlockedChanged)
   Q_PROPERTY(QString connectionError READ connectionError NOTIFY errorConnecting)
   Q_PROPERTY(QString openError READ openError NOTIFY errorOpening)
   Q_PROPERTY(QString unlockError READ unlockError NOTIFY errorUnlocking)
   Q_PROPERTY(QString brainKey READ brainKey NOTIFY brainKeyChanged)

public:
   LightWallet()
      : m_walletThread("Wallet Implementation Thread")
   {}
   ~LightWallet(){}

   bool walletExists() const;
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

   QString brainKey() const
   {
      return m_brainKey;
   }

public Q_SLOTS:
   void connectToServer( QString host, uint16_t port = 0,
                         QString user = QString("any"),
                         QString password = QString("none") );
   void disconnectFromServer();

   void createWallet(QString password);
   void openWallet();
   void closeWallet();

   void unlockWallet(QString password);
   void lockWallet();

   void clearBrainKey();

Q_SIGNALS:
   void walletExistsChanged(bool exists);
   void errorConnecting(QString error);
   void errorOpening(QString error);
   void errorUnlocking(QString arg);
   void connectedChanged(bool connected);
   void openChanged(bool open);
   void unlockedChanged(bool unlocked);
   void brainKeyChanged(QString arg);

private:
   fc::thread m_walletThread;
   bts::light_wallet::light_wallet m_wallet;
   QString m_connectionError;
   QString m_openError;
   QString m_unlockError;
   QString m_brainKey;

   void generateBrainKey();
};
