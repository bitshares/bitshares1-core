#include <bts/blockchain/types.hpp>

#include <QStringList>

#include "Account.hpp"
#include "QtConversions.hpp"

using std::string;

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

QList<QObject*> Account::transactionHistory(QString asset_symbol)
{
   QList<QObject*> history;

   for( bts::wallet::transaction_ledger_entry trx : m_wallet->transactions(convert(asset_symbol)) )
   {
      TransactionSummary* summary = ledgerMaster->findChild<TransactionSummary*>(convert(string(trx.id)));
      if( summary == nullptr )
      {
         QList<LedgerEntry*> ledger;

         for( auto label : trx.delta_labels )
         {
            QStringList participants = convert(label.second).split('>');
            if( participants.size() == 1 ) participants.prepend(tr("Unknown"));

            for( bts::blockchain::asset amount : trx.delta_amounts[label.second] )
            {
               auto asset = m_wallet->get_asset_record(amount.asset_id);
               if( !asset ) continue;

               LedgerEntry* entry = new LedgerEntry(summary);

               entry->setProperty("sender", participants.first());
               entry->setProperty("receiver", participants.last());
               entry->setProperty("amount", double(amount.amount) / asset->precision);
               entry->setProperty("symbol", convert(asset->symbol));
               if( trx.operation_notes.count(label.first) )
                  entry->setProperty("memo", convert(trx.operation_notes[label.first]));

               ledger.append(entry);
            }
         }

         if( trx.delta_amounts.count("Fee") )
         {
            for( bts::blockchain::asset fee : trx.delta_amounts["Fee"] )
            {
               auto asset = m_wallet->get_asset_record(fee.asset_id);
               if( !asset ) continue;

               LedgerEntry* entry = new LedgerEntry(summary);
               entry->setProperty("receiver", tr("Network"));
               entry->setProperty("amount", double(fee.amount) / asset->precision);
               entry->setProperty("symbol", convert(asset->symbol));
               entry->setProperty("memo", tr("Transaction Fee"));

               ledger.append(entry);
            }
         }

         summary = new TransactionSummary(convert(string(trx.id)), convert(trx.timestamp), std::move(ledger), ledgerMaster);
         summary->setObjectName(convert(string(trx.id)));
      }

      history.append(summary);
   }

   std::sort(history.begin(), history.end(), [](QObject* first, QObject* second) {
      return first->property("timestamp").toDateTime() < second->property("timestamp").toDateTime();
   });
   return history;
}
