#pragma once

#include <map>

#include <bts/blockchain/types.hpp>

const static std::map<uint32_t, bts::blockchain::block_id_type> CHECKPOINT_BLOCKS {
      {40634, bts::blockchain::block_id_type("d8fdb417500bb8efa4725a529bb695312475edaa")}
};
