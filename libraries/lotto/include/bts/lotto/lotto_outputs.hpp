#pragma once
#include <bts/blockchain/outputs.hpp>

namespace bts { namespace lotto {
   

enum claim_type_enum
{
   /** basic claim by single address */
   claim_ticket         = 30,
};


struct claim_ticket_input
{
    static const claim_type_enum type;
};

struct claim_ticket_output
{
    static const claim_type_enum type;
    
    claim_ticket_output():odds(1),lucky_number(0){}

    /**
     *  This is the number chosen by the user or at 
     *  random, ie: their lotto ticket number.
     */
    uint64_t                   lucky_number;

    /**
     *  Who owns the ticket and thus can claim the jackpot
     */
    bts::blockchain::address   owner;

    /** The probability of winning... increasing the odds will 
     * cause the amount won to grow by Jackpot * odds, but the
     * probability of winning decreases by 2*odds.
     */
    uint16_t                   odds; 
};

}} //bts::lotto

FC_REFLECT_ENUM(bts::lotto::claim_type_enum, (claim_ticket));
FC_REFLECT(bts::lotto::claim_ticket_input, BOOST_PP_SEQ_NIL);
FC_REFLECT(bts::lotto::claim_ticket_output, (owner)(odds));
