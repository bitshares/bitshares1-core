#pragma once
#include <bts/blockchain/output_reference.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/outputs.hpp>
#include <bts/blockchain/small_hash.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/io/varint.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

typedef uint160 transaction_id_type;

/**
 *  @class trx_input
 *  @brief references an unspent output and provides data required to spend it.
 *
 *  Defines the source of an input used 
 *  as part of a transaction.  If must first
 *  reference an existing unspent output and
 *  then provide the input data required to
 *  cause the claim function to evaluate true.
 */
struct trx_input
{
    trx_input(){}
    trx_input( const output_reference& src )
    :output_ref(src)
    { }

    template<typename InputType>
    trx_input( const InputType& t, const output_reference& src )
    :output_ref(src)
    {
       input_data = fc::raw::pack(t);
    }

    template<typename InputType>
    InputType as()const
    {
       return fc::raw::unpack<InputType>(input_data);
    }

    output_reference   output_ref;
    std::vector<char>  input_data;
};


/**
 *  @class trx_output
 *  @brief specifices an output balance and the conditions by which it may be claimed
 *
 *  Base of all trx outputs, derived classes will define the extra
 *  data associated with the claim_func.  The goal is to enable the
 *  output types to be 'fixed size' and thus each output type would
 *  have its own 'memory pool' in the chain state data structure. This
 *  has the added benefit of conserving space, separating bids/asks/
 *  escrow/and normal transfers into different memory segments and
 *  should give better memory performance.   
 */
struct trx_output
{
    template<typename ClaimType>
    trx_output( const ClaimType& t, const asset& a )
    :amount(a)
    {
       claim_func = ClaimType::type;
       claim_data = fc::raw::pack(t);
    }

    template<typename ClaimType>
    ClaimType as()const
    {
       FC_ASSERT( claim_func == ClaimType::type, "", ("claim_func",claim_func)("ClaimType",ClaimType::type) );
       return fc::raw::unpack<ClaimType>(claim_data);
    }

    trx_output(){}

    asset                                       amount;
    claim_type                                  claim_func;
    std::vector<char>                           claim_data;
};

/**
 *  @class trx_num
 *  @brief References a transaction by its location in the blockchain
 *
 *  The location in the blockchain is given by the block number and index of
 *  the transaction.  
 */
struct trx_num
{
    /** 
     *  -1 block_num is used to identifiy default initialization.
     */
    static const uint32_t  invalid_block_num  = uint32_t(-1);
    static const uint16_t  invalid_trx_idx    = uint16_t(-1);
    static const uint8_t   invalid_output_num = uint8_t(-1);

    trx_num(uint32_t b = invalid_block_num, uint16_t t = invalid_trx_idx)
    :block_num(b),trx_idx(t){}

    uint32_t block_num;
    uint16_t trx_idx;
  
    friend bool operator < ( const trx_num& a, const trx_num& b )
    {
      return a.block_num == b.block_num ? 
             a.trx_idx   <  b.trx_idx   : 
             a.block_num <  b.block_num ;
    }
    friend bool operator == ( const trx_num& a, const trx_num& b )
    {
      return a.block_num == b.block_num && a.trx_idx == b.trx_idx;
    }
};



/**
 *  @class meta_trx_output 
 *  @brief extra data about each output, such as where it was spent
 *
 */
struct meta_trx_output
{
   meta_trx_output()
   :input_num(trx_num::invalid_output_num){}
   trx_num   trx_id;
   uint8_t   input_num; // TODO: define -1 as the constant for 

   bool is_spent()const 
   {
     return trx_id.block_num != trx_num::invalid_block_num;
   }
};


/**
 *  @class meta_trx_input
 *
 *  Caches output information used by inputs while
 *  evaluating a transaction.
 */
struct meta_trx_input
{
   meta_trx_input()
   :output_num(-1){}

   trx_num           source;
   uint32_t          output_num;
   fc::signed_int    delegate_id;
   trx_output        output;
   meta_trx_output   meta_output;
   std::vector<char> data;
};


/**
 *  @class transaction
 *  @brief maps inputs to outputs.
 *
 */
struct transaction
{
   transaction():version(0),stake(0){}
   fc::sha256                   digest()const;

   uint8_t                      version;
   int32_t                      vote;           ///< delegate_id outputs of this transaction are voting for
   uint32_t                     stake;          ///< used for proof of stake, last 8 bytes of block.id()
   fc::time_point_sec           valid_until;    ///< trx is only valid until a given time
   std::vector<trx_input>       inputs;
   std::vector<trx_output>      outputs;

};
/**
 *  @class signed_transaction
 *  @brief a transaction with signatures required by the inputs
 */
struct signed_transaction : public transaction
{
    std::unordered_set<address>      get_signed_addresses()const;
    std::unordered_set<pts_address>  get_signed_pts_addresses()const;
    transaction_id_type              id()const;
    void                             sign( const fc::ecc::private_key& k );
    size_t                           size()const;

    std::set<fc::ecc::compact_signature> sigs;
};

typedef std::vector<signed_transaction> signed_transactions;

/**
 *  @class meta_trx 
 *  @brief addes meta information about all outputs of a signed transaction 
 */
struct meta_trx : public signed_transaction
{
   meta_trx(){}
   meta_trx( const signed_transaction& t )
   :signed_transaction(t), meta_outputs(t.outputs.size()){}

   /** @note the order of these outputs is the same as the order of the
    * signed_transaction::outputs
    */
   std::vector<meta_trx_output> meta_outputs; // tracks where the output was spent
};


} }  // namespace bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo );
};


FC_REFLECT( bts::blockchain::trx_input, (output_ref)(input_data) )
FC_REFLECT( bts::blockchain::trx_output, (amount)(claim_func)(claim_data) )
FC_REFLECT( bts::blockchain::transaction, (version)(stake)(vote)(valid_until)(inputs)(outputs) )
FC_REFLECT_DERIVED( bts::blockchain::signed_transaction, (bts::blockchain::transaction), (sigs) );
FC_REFLECT( bts::blockchain::meta_trx_output, (trx_id)(input_num) )
FC_REFLECT( bts::blockchain::meta_trx_input, (source)(output_num)(delegate_id)(output)(meta_output)(data) )
FC_REFLECT_DERIVED( bts::blockchain::meta_trx, (bts::blockchain::signed_transaction), (meta_outputs) );

