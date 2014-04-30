#include <bts/dns/dns_outputs.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace dns {

const claim_type_enum claim_dns_input::type  = claim_type_enum::claim_dns;
const claim_type_enum claim_dns_output::type = claim_type_enum::claim_dns;

claim_dns_output::claim_dns_output()
{
}

claim_dns_output::claim_dns_output(const std::string& k, const last_tx_type_enum& l, const address& o,
                                   const std::vector<char>& v) : key(k), last_tx_type(l), owner(o), value(v)
{
}

bool is_dns_output(const trx_output& output)
{
    return output.claim_func == claim_dns;
}

claim_dns_output to_dns_output(const trx_output& output)
{
    return output.as<claim_dns_output>();
}

std::vector<char> serialize_value(const fc::variant& value)
{
    return fc::raw::pack(value);
}

fc::variant unserialize_value(const std::vector<char>& value)
{
    return fc::raw::unpack<fc::variant>(value);
}

} } // bts::dns
