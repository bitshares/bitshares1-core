#pragma once

#include <QObject>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#include <bts/light_wallet/light_wallet.hpp>

#include <fc/thread/thread.hpp>

#include "QtWrappers.hpp"
#include "Account.hpp"

class LightWallet : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool walletExists READ walletExists NOTIFY walletExistsChanged STORED false)
   Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
   Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
   Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY unlockedChanged)
   Q_PROPERTY(Account* account READ account NOTIFY accountChanged)
   Q_PROPERTY(QString connectionError READ connectionError NOTIFY errorConnecting)
   Q_PROPERTY(QString openError READ openError NOTIFY errorOpening)
   Q_PROPERTY(QString unlockError READ unlockError NOTIFY errorUnlocking)
   Q_PROPERTY(QString brainKey READ brainKey NOTIFY brainKeyChanged)

public:
   LightWallet()
      : m_walletThread("Wallet Implementation Thread")
   {
      auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
      qDebug() << "Creating data directory:" << path;
      QDir(path).mkpath(".");
   }
   virtual ~LightWallet()
   {
      m_walletThread.quit();
      m_wallet.save();
   }

   bool walletExists() const;
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
   QString brainKey() const
   {
      return m_brainKey;
   }
   Account* account() const
   {
      return m_account;
   }

public Q_SLOTS:
   void connectToServer( QString host, quint16 port,
                         QString user = QString("any"),
                         QString password = QString("none") );
   void disconnectFromServer();

   void createWallet(QString accountName, QString password);
   void openWallet();
   void closeWallet();

   void unlockWallet(QString password);
   void lockWallet();

   void registerAccount();

   void clearBrainKey();

   void sync();

Q_SIGNALS:
   void walletExistsChanged(bool exists);
   void errorConnecting(QString error);
   void errorOpening(QString error);
   void errorUnlocking(QString error);
   void errorRegistering(QString error);
   void connectedChanged(bool connected);
   void openChanged(bool open);
   void unlockedChanged(bool unlocked);
   void brainKeyChanged(QString key);
   void accountChanged(Account* arg);

   void synced();

private:
   fc::thread m_walletThread;
   QString m_dataDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
   fc::path m_walletPath = fc::path(m_dataDir.toStdWString())
         / "wallet_data.json";
   bts::light_wallet::light_wallet m_wallet;
   QString m_connectionError;
   QString m_openError;
   QString m_unlockError;
   QString m_brainKey;
   Account* m_account = nullptr;

   void generateBrainKey();
   void updateAccount(const bts::light_wallet::account_record& account);
};
