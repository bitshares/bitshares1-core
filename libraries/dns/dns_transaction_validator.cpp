#include <bts/dns/dns_transaction_validator.hpp>

namespace bts { namespace dns {

dns_transaction_validator::dns_transaction_validator(dns_db* db) : bts::blockchain::transaction_validator(db), _dns_db(db)
{
    FC_ASSERT(db != NULL);
}

dns_transaction_validator::~dns_transaction_validator()
{
}

} } // bts::dns
