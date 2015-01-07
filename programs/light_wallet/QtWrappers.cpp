#include "QtWrappers.hpp"

Account::Account(const bts::blockchain::account_record& account, QObject* parent)
   : QObject(parent),
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
