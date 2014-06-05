# Warning: this is a generated file, any changes made here will be overwritten by the build process

class BlockchainAPI

  constructor: (@q, @log, @rpc) ->
    #@log.info "---- Network API Constructor ----"


  # Returns hash of block in best-block-chain at index provided
  # parameters: 
  #   uint32_t `block_number` - index of the block, example: 42
  # return_type: `block_id_type`
  blockchain_get_blockhash: (block_number) ->
    @rpc.request('blockchain_get_blockhash', ['block_number']).then (response) ->
      response.result

  # Returns the number of blocks in the longest block chain
  # parameters: 
  # return_type: `uint32_t`
  blockchain_get_blockcount:  ->
    @rpc.request('blockchain_get_blockcount').then (response) ->
      response.result

  # Returns registered accounts starting with a given name upto a the limite provided
  # parameters: 
  #   account_name `first_account_name` - the first account name to include
  #   int64_t `limit` - the maximum number of items to list
  # return_type: `account_record_array`
  blockchain_list_registered_accounts: (first_account_name, limit) ->
    @rpc.request('blockchain_list_registered_accounts', ['first_account_name', 'limit']).then (response) ->
      response.result

  # Return a list of transactions that are not yet in a block.
  # parameters: 
  # return_type: `signed_transaction_array`
  blockchain_get_pending_transactions:  ->
    @rpc.request('blockchain_get_pending_transactions').then (response) ->
      response.result



angular.module("app").service("BlockchainAPI", ["$q", "$log", "RpcService", BlockchainAPI])
