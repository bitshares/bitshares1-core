#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

#define BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE    (BTS_BLOCKCHAIN_BLOCKS_PER_DAY / 12)
#define BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE       (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14)

namespace bts { namespace blockchain {

share_type chain_interface::get_delegate_registration_fee_v1( uint8_t pay_rate )const
{
   const share_type unscaled_fee = (get_delegate_pay_rate_v1() * BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE)/BTS_BLOCKCHAIN_NUM_DELEGATES;
   return (unscaled_fee * pay_rate) / 100;
}

share_type chain_interface::get_asset_registration_fee_v1()const
{
   return (get_delegate_pay_rate_v1() * BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE);
}

share_type chain_interface::get_delegate_pay_rate_v1()const
{
    const auto base_record = get_asset_record( asset_id_type( 0 ) );
    FC_ASSERT( base_record.valid() );
    return base_record->collected_fees / (BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 14);
}

} } // bts::blockchain
