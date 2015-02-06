#pragma once

#include <bts/light_wallet/light_wallet.hpp>

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QQmlListProperty>

#include "QtWrappers.hpp"

class Account : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
   Q_PROPERTY(QString ownerKey READ ownerKey WRITE setOwnerKey NOTIFY ownerKeyChanged)
   Q_PROPERTY(QString activeKey READ activeKey WRITE setActiveKey NOTIFY activeKeyChanged)
   Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY isRegisteredChanged)
   Q_PROPERTY(QDateTime registrationDate READ registrationDate NOTIFY registrationDateChanged)
   Q_PROPERTY(QQmlListProperty<Balance> balances READ balances NOTIFY balancesChanged)
   Q_PROPERTY(QStringList availableAssets READ availableAssets NOTIFY balancesChanged STORED false)

   bts::light_wallet::light_wallet* m_wallet;

   QString m_name;
   QString m_ownerKey;
   QString m_activeKey;
   bool m_isRegistered;
   QDateTime m_registrationDate;
   QList<Balance*> balanceList;
   QMap<QString,QObjectList> transactionList;

   fc::variant_object pending_transaction;

   TransactionSummary* buildSummary(bts::wallet::transaction_ledger_entry trx);

public:
   Account(bts::light_wallet::light_wallet* wallet,
           const bts::light_wallet::account_record& account,
           QObject* parent = nullptr);
   virtual ~Account(){}

   Account& operator= (const bts::light_wallet::account_record& account);

   QString name() const
   {
      return m_name;
   }
   QString ownerKey() const
   {
      return m_ownerKey;
   }
   QString activeKey() const
   {
      return m_activeKey;
   }
   bool isRegistered() const
   {
      return m_isRegistered;
   }
   QDateTime registrationDate() const
   {
      return m_registrationDate;
   }
   QQmlListProperty<Balance> balances();
   Q_INVOKABLE Balance* balance(QString symbol);
   Q_INVOKABLE QStringList availableAssets();

   Q_INVOKABLE QList<QObject*> transactionHistory(QString asset_symbol);

   Q_INVOKABLE TransactionSummary* beginTransfer(QString recipient, QString amount, QString symbol, QString memo);
   Q_INVOKABLE bool completeTransfer(QString password);

public Q_SLOTS:
   void setName(QString arg)
   {
      if (m_name == arg)
         return;

      m_name = arg;
      Q_EMIT nameChanged(arg);
   }
   void setOwnerKey(QString arg)
   {
      if (m_ownerKey == arg)
         return;

      m_ownerKey = arg;
      Q_EMIT ownerKeyChanged(arg);
   }
   void setActiveKey(QString arg)
   {
      if (m_activeKey == arg)
         return;

      m_activeKey = arg;
      Q_EMIT activeKeyChanged(arg);
   }

Q_SIGNALS:
   void nameChanged(QString arg);
   void ownerKeyChanged(QString arg);
   void activeKeyChanged(QString arg);
   void isRegisteredChanged(bool arg);
   void registrationDateChanged(QDateTime arg);
   void balancesChanged();

   void error(QString errorString);
};
