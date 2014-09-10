#pragma once
#include <bts/blockchain/genesis_config.hpp>
#include <sstream>

namespace bts { namespace blockchain {

#ifdef EMBED_GENESIS_STATE_AS_TEXT
  std::string get_builtin_genesis_json_as_string();
#else
  genesis_block_config get_builtin_genesis_block_config();
  fc::sha256 get_builtin_genesis_block_state_hash();
#endif

} } // bts::blockchain