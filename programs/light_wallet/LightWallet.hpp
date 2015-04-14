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
   Q_PROPERTY(QStringList accountNames READ accountNames NOTIFY accountsChanged)
   Q_PROPERTY(QVariantMap accounts READ accounts NOTIFY accountsChanged)
   Q_PROPERTY(QStringList allAssets READ allAssets NOTIFY allAssetsChanged)
   Q_PROPERTY(QString connectionError READ connectionError NOTIFY errorConnecting)
   Q_PROPERTY(QString openError READ openError NOTIFY errorOpening)
   Q_PROPERTY(QString unlockError READ unlockError NOTIFY errorUnlocking)
   Q_PROPERTY(QString brainKey READ brainKey NOTIFY brainKeyChanged)

   Q_PROPERTY(qint16 maxMemoSize READ maxMemoSize CONSTANT)
   Q_PROPERTY(QString baseAssetSymbol READ baseAssetSymbol CONSTANT)
   Q_PROPERTY(QString keyPrefix READ keyPrefix CONSTANT)

public:
   LightWallet();
   virtual ~LightWallet()
   {
      if( walletExists() )
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

   Q_INVOKABLE static QString sha256(QString text) {
      return convert(fc::sha256::hash(convert(text)).str());
   }

   Q_INVOKABLE Balance* getFee(QString assetSymbol);
   Q_INVOKABLE int getDigitsOfPrecision(QString assetSymbol);
   Q_INVOKABLE bool accountExists(QString name);
   Q_INVOKABLE bool isValidAccountName(QString name)
   {
      return bts::blockchain::chain_database::is_valid_account_name(convert(name));
   }
   Q_INVOKABLE bool isValidKey(QString key)
   {
      try {
         return bts::blockchain::public_key_type(convert(key)) != bts::blockchain::public_key_type();
      } catch (...) {
         return false;
      }
   }

   qint16 maxMemoSize() const
   {
      return BTS_BLOCKCHAIN_MAX_EXTENDED_MEMO_SIZE;
   }
   QString baseAssetSymbol() const
   {
      return QStringLiteral(BTS_BLOCKCHAIN_SYMBOL);
   }
   QString keyPrefix() const
   {
      return QStringLiteral(BTS_ADDRESS_PREFIX);
   }

   Q_INVOKABLE bool verifyBrainKey(QString key) const;

   QStringList accountNames()
   {
      QStringList results;
      if( !isOpen() )
         return results;
      for( auto account : m_wallet.account_records() )
         results.append(convert(account->name));
      return results;
   }
   QVariantMap accounts() const
   {
      return m_accounts;
   }

   QStringList allAssets();

public Q_SLOTS:
   void pollForRegistration(QString accountName);

   void connectToServer(QString host, quint16 port,
                        QString serverKey = QString(),
                        QString user = QString("any"),
                        QString password = QString("none"));
   void disconnectFromServer();

   void createWallet(QString accountName, QString password);
   QStringList recoverableAccounts(QString brainKey);
   bool recoverWallet(QString accountName, QString password, QString brainKey);
   void openWallet();
   void closeWallet();

   void unlockWallet(QString password);
   void lockWallet();

   void registerAccount(QString accountName);

   void clearBrainKey();

   void sync();
   void syncAllBalances();
   void syncAllTransactions();

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

   void synced();

   void notification(QString message);

   void accountsChanged(QVariantMap arg);
   void allAssetsChanged();

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
   QVariantMap m_accounts;

   mutable QMap<QString,int> m_digitsOfPrecisionCache;

   void generateBrainKey(uint8_t wordCount = 16);
   void updateAccount(const bts::light_wallet::account_record& account);
};
