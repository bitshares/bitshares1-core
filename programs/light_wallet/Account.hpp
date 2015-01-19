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
   Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY isRegisteredChanged)
   Q_PROPERTY(QDateTime registrationDate READ registrationDate NOTIFY registrationDateChanged)
   Q_PROPERTY(QQmlListProperty<Balance> balances READ balances NOTIFY balancesChanged)
   Q_PROPERTY(QStringList availableAssets READ availableAssets NOTIFY balancesChanged STORED false)

   bts::light_wallet::light_wallet* m_wallet;

   QString m_name;
   bool m_isRegistered;
   QDateTime m_registrationDate;
   QObject* balanceMaster = new QObject(this);
   QObject* ledgerMaster = new QObject(this);

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
   bool isRegistered() const
   {
      return m_isRegistered;
   }
   QDateTime registrationDate() const
   {
      return m_registrationDate;
   }
   QQmlListProperty<Balance> balances();
   Q_INVOKABLE qreal balance(QString symbol);
   Q_INVOKABLE QStringList availableAssets();

   Q_INVOKABLE QList<QObject*> transactionHistory(QString asset_symbol);

public Q_SLOTS:
   void setName(QString arg)
   {
      if (m_name == arg)
         return;

      m_name = arg;
      Q_EMIT nameChanged(arg);
   }

Q_SIGNALS:
   void nameChanged(QString arg);
   void isRegisteredChanged(bool arg);
   void registrationDateChanged(QDateTime arg);
   void balancesChanged();
};
