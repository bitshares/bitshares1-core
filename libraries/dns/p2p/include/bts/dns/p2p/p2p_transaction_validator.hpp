#pragma once

#include <bts/dns/dns_transaction_validator.hpp>

#define DNS_DEBUG

#define DNS_MAX_KEY_LEN             128
#define DNS_MAX_VALUE_LEN           1024

#ifndef DNS_DEBUG
#define DNS_AUCTION_DURATION_BLOCKS 864    /* Blocks from last bid */
#define DNS_EXPIRE_DURATION_BLOCKS  105120 /* Blocks from max(end of auction, last update) */
#else
#define DNS_AUCTION_DURATION_BLOCKS 3      /* Blocks from last bid */
#define DNS_EXPIRE_DURATION_BLOCKS  5      /* Blocks from max(end of auction, last update) */
#endif

#define DNS_MIN_BID_FROM(bid)       ((11 * (bid)) / 10)
#define DNS_BID_FEE_RATIO(bid)      ((bid) / 2)

namespace bts { namespace dns { namespace p2p {

using namespace bts::blockchain;

struct p2p_transaction_evaluation_state : public bts::blockchain::transaction_evaluation_state
{
    p2p_transaction_evaluation_state(const signed_transaction& tx)
    : transaction_evaluation_state(tx), seen_dns_input(false), seen_dns_output(false) {}

    claim_dns_output dns_input;
    asset dns_input_amount;

    /* Only one dns input/output per tx */
    bool seen_dns_input;
    bool seen_dns_output;
};

struct p2p_block_evaluation_state : public bts::blockchain::block_evaluation_state
{
    p2p_block_evaluation_state(dns_db* db) : bts::blockchain::block_evaluation_state(db) {}

    std::vector<std::string> pending_keys;
};

typedef std::shared_ptr<p2p_block_evaluation_state> p2p_block_evaluation_state_ptr;

class p2p_transaction_validator : public bts::dns::dns_transaction_validator
{
    public:
        p2p_transaction_validator(dns_db* db);
        virtual ~p2p_transaction_validator() override;

        virtual block_evaluation_state_ptr create_block_state() const override;

        virtual transaction_summary evaluate(const signed_transaction& tx,
                                             const block_evaluation_state_ptr& block_state) override;

        virtual void validate_input(const meta_trx_input& in, transaction_evaluation_state& state,
                                    const block_evaluation_state_ptr& block_state) override;

        virtual void validate_output(const trx_output& out, transaction_evaluation_state& state,
                                     const block_evaluation_state_ptr& block_state) override;

        virtual bool is_valid_output(const claim_dns_output& output) override;
        virtual bool is_valid_key(const std::string& key) override;
        virtual bool is_valid_value(const std::vector<char>& value) override;

        virtual bool is_valid_bid_price(const asset& bid_price, const asset& prev_bid_price) override;
        virtual asset get_bid_transfer_amount(const asset& bid_price, const asset& prev_bid_price) override;

        virtual bool auction_is_closed(const output_reference& output_ref) override;
        virtual bool key_is_expired(const output_reference& output_ref) override;

        virtual bool key_is_available(const std::string& key, const std::vector<std::string>& pending_keys,
                                      bool& new_or_expired, output_reference& prev_output_ref) override;
        virtual bool key_is_useable(const std::string& key, const std::vector<std::string>& pending_keys,
                                    const std::vector<std::string>& unspent_keys, output_reference& prev_output_ref) override;

    private:
        void validate_p2p_input(const claim_dns_output& input, const asset& amount,
                                p2p_transaction_evaluation_state& state,
                                const p2p_block_evaluation_state_ptr& block_state);

        void validate_p2p_output(const claim_dns_output& output, const asset& amount,
                                 p2p_transaction_evaluation_state& state,
                                 const p2p_block_evaluation_state_ptr& block_state);
};

} } } // bts::dns::p2p

FC_REFLECT(bts::dns::p2p::p2p_transaction_evaluation_state, (seen_dns_input)(seen_dns_output)(dns_input)(dns_input_amount));
FC_REFLECT(bts::dns::p2p::p2p_block_evaluation_state, (pending_keys));
