#include <bts/blockchain/types.hpp>

#include "Account.hpp"
#include "QtConversions.hpp"

Account::Account(bts::light_wallet::light_wallet* wallet,
                 const bts::blockchain::account_record& account,
                 QObject* parent)
   : QObject(parent),
     m_wallet(wallet),
     m_name(convert(account.name)),
     m_isRegistered(account.registration_date != fc::time_point_sec()),
     m_registrationDate(convert(account.registration_date))
{}

Account& Account::operator=(const bts::blockchain::account_record& account)
{
   m_name = convert(account.name);
   m_isRegistered = (account.registration_date != fc::time_point_sec());
   m_registrationDate = convert(account.registration_date);
   Q_EMIT nameChanged(m_name);
   Q_EMIT isRegisteredChanged(m_isRegistered);
   Q_EMIT registrationDateChanged(m_registrationDate);

   return *this;
}

QQmlListProperty<Balance> Account::balances()
{
   auto count = [](QQmlListProperty<Balance>* list) -> int {
      return static_cast<Account*>(list->data)->m_wallet->balance().size();
   };
   auto at = [](QQmlListProperty<Balance>* list, int index) -> Balance* {
      auto account = static_cast<Account*>(list->data);
      auto balances = account->m_wallet->balance();
      auto itr = balances.begin();
      for( int i = 0; i < index; ++i )
         ++itr;

      Balance* balance = account->balanceMaster->findChild<Balance*>(convert(itr->first));
      if( balance == nullptr )
      {
         balance = new Balance(convert(itr->first), itr->second, account->balanceMaster);
         balance->setObjectName(convert(itr->first));
      } else
         balance->setProperty("amount", itr->second);

      return balance;
   };

   return QQmlListProperty<Balance>(balanceMaster, this, count, at);
}
