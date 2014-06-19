# Warning: this is a generated file, any changes made here will be overwritten by the build process

class BlockchainAPI

  constructor: (@q, @log, @rpc) ->
    #@log.info "---- Network API Constructor ----"


  # Returns hash of block in best-block-chain at index provided
  # parameters: 
  #   uint32_t `block_number` - index of the block, example: 42
  # return_type: `block_id_type`
  get_blockhash: (block_number) ->
    @rpc.request('blockchain_get_blockhash', [block_number]).then (response) ->
      response.result

  # Returns the number of blocks in the longest block chain
  # parameters: 
  # return_type: `uint32_t`
  get_blockcount:  ->
    @rpc.request('blockchain_get_blockcount').then (response) ->
      response.result

  # Returns information about blockchain security level
  # parameters: 
  # return_type: `blockchain_security_state`
  get_security_state:  ->
    @rpc.request('blockchain_get_security_state').then (response) ->
      response.result

  # Returns registered accounts starting with a given name upto a the limit provided
  # parameters: 
  #   account_name `first_account_name` - the first account name to include
  #   int32_t `limit` - the maximum number of items to list
  # return_type: `account_record_array`
  list_registered_accounts: (first_account_name, limit) ->
    @rpc.request('blockchain_list_registered_accounts', [first_account_name, limit]).then (response) ->
      response.result

  # Returns registered assets starting with a given name upto a the limit provided
  # parameters: 
  #   asset_symbol `first_symbol` - the prefix of the first asset symbol name to include
  #   int32_t `limit` - the maximum number of items to list
  # return_type: `asset_record_array`
  list_registered_assets: (first_symbol, limit) ->
    @rpc.request('blockchain_list_registered_assets', [first_symbol, limit]).then (response) ->
      response.result

  # Return a list of transactions that are not yet in a block.
  # parameters: 
  # return_type: `signed_transaction_array`
  get_pending_transactions:  ->
    @rpc.request('blockchain_get_pending_transactions').then (response) ->
      response.result

  # Get detailed information about an in-wallet transaction
  # parameters: 
  #   string `transaction_id` - the base58 transaction ID to return
  #   bool `exact` - whether or not a partial match is ok
  # return_type: `optional_blockchain_transaction_record`
  get_transaction: (transaction_id, exact) ->
    @rpc.request('blockchain_get_transaction', [transaction_id, exact]).then (response) ->
      response.result

  # Retrieves the block header for the given block hash
  # parameters: 
  #   block_id_type `block_id` - the id of the block to return
  # return_type: `digest_block`
  get_block: (block_id) ->
    @rpc.request('blockchain_get_block', [block_id]).then (response) ->
      response.result

  # Retrieves the detailed transaction information for a block
  # parameters: 
  #   block_id_type `block_id` - the id of the block to return
  # return_type: `blockchain_transaction_record_array`
  get_transactions_for_block: (block_id) ->
    @rpc.request('blockchain_get_transactions_for_block', [block_id]).then (response) ->
      response.result

  # Retrieves the block header for the given block number
  # parameters: 
  #   uint32_t `block_number` - the number of the block to return
  # return_type: `digest_block`
  get_block_by_number: (block_number) ->
    @rpc.request('blockchain_get_block_by_number', [block_number]).then (response) ->
      response.result

  # Retrieves the name record
  # parameters: 
  #   account_name `account_name` - the name of the account retrieve
  # return_type: `optional_account_record`
  get_account_record: (account_name) ->
    @rpc.request('blockchain_get_account_record', [account_name]).then (response) ->
      response.result

  # Retrieves the name record for the given account_id
  # parameters: 
  #   account_id_type `account_id` - the id of the name record to retrieve
  # return_type: `optional_account_record`
  get_account_record_by_id: (account_id) ->
    @rpc.request('blockchain_get_account_record_by_id', [account_id]).then (response) ->
      response.result

  # Retrieves the asset record by the ticker symbol
  # parameters: 
  #   asset_symbol `symbol` - the ticker symbol to retrieve
  # return_type: `optional_asset_record`
  get_asset_record: (symbol) ->
    @rpc.request('blockchain_get_asset_record', [symbol]).then (response) ->
      response.result

  # Retrieves the asset record by the id
  # parameters: 
  #   asset_id_type `asset_id` - the id of the asset to retrieve
  # return_type: `optional_asset_record`
  get_asset_record_by_id: (asset_id) ->
    @rpc.request('blockchain_get_asset_record_by_id', [asset_id]).then (response) ->
      response.result

  # Returns the list of delegates sorted by vote
  # parameters: 
  #   uint32_t `first` - 
  #   uint32_t `count` - 
  # return_type: `account_record_array`
  list_delegates: (first, count) ->
    @rpc.request('blockchain_list_delegates', [first, count]).then (response) ->
      response.result

  # Returns the list of proposals
  # parameters: 
  #   uint32_t `first` - 
  #   uint32_t `count` - 
  # return_type: `proposal_array`
  list_proposals: (first, count) ->
    @rpc.request('blockchain_list_proposals', [first, count]).then (response) ->
      response.result

  # Returns the list votes filed for a given proposal.
  # parameters: 
  #   proposal_id `proposal_id` - The ID of the proposal that votes will be returned for.
  # return_type: `proposal_vote_array`
  get_proposal_votes: (proposal_id) ->
    @rpc.request('blockchain_get_proposal_votes', [proposal_id]).then (response) ->
      response.result

  # Returns the bid side of the order book for a given market
  # parameters: 
  #   asset_symbol `quote_symbol` - the symbol name the market is quoted in
  #   asset_symbol `base_symbol` - the item being bought in this market
  #   int64_t `limit` - the maximum number of items to return, -1 for all
  # return_type: `market_order_array`
  market_list_bids: (quote_symbol, base_symbol, limit) ->
    @rpc.request('blockchain_market_list_bids', [quote_symbol, base_symbol, limit]).then (response) ->
      response.result

  # returns the order of delegates that is fixed for the current round
  # parameters: 
  # return_type: `account_id_array`
  list_current_round_active_delegates:  ->
    @rpc.request('blockchain_list_current_round_active_delegates').then (response) ->
      response.result

  # Returns the block headers for blocks in a range
  # parameters: 
  #   int32_t `first_block_number` - the first block to list
  #   uint32_t `limit` - the maximum number of blocks to return
  # return_type: `block_record_array`
  list_blocks: (first_block_number, limit) ->
    @rpc.request('blockchain_list_blocks', [first_block_number, limit]).then (response) ->
      response.result

  # dumps the fork data to graphviz format
  # parameters: 
  #   uint32_t `start_block` - the first block number to consider
  #   uint32_t `end_block` - the last block number to consider
  #   string `filename` - the filename to save to
  # return_type: `std::string`
  export_fork_graph: (start_block, end_block, filename) ->
    @rpc.request('blockchain_export_fork_graph', [start_block, end_block, filename]).then (response) ->
      response.result

  # returns a list of all blocks for which there is a fork off of the main chain
  # parameters: 
  # return_type: `uint32_t_array`
  list_forks:  ->
    @rpc.request('blockchain_list_forks').then (response) ->
      response.result

  # Query the block production stats for a particular delegate
  # parameters: 
  #   account_id_type `delegate_id` - Delegate whose block stats to query
  # return_type: `delegate_block_stats_map`
  get_delegate_block_stats: (delegate_id) ->
    @rpc.request('blockchain_get_delegate_block_stats', [delegate_id]).then (response) ->
      response.result

  # Get the delegate that signed a given block
  # parameters: 
  #   uint32_t `block_number` - Block whose signing delegate to return
  # return_type: `string`
  get_signing_delegate: (block_number) ->
    @rpc.request('blockchain_get_signing_delegate', [block_number]).then (response) ->
      response.result



angular.module("app").service("BlockchainAPI", ["$q", "$log", "RpcService", BlockchainAPI])
