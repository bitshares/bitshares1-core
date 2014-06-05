# Warning: this is a generated file, any changes made here will be overwritten by the build process

class WalletAPI

  constructor: (@q, @log, @rpc, @interval) ->
    #@log.info "---- WalletAPI Constructor ----"


  # Opens the wallet at the given path
  # parameters: 
  #   filename `wallet_file` - the full path and filename of the wallet to open, example: /path/to/wallet.dat
  # return_type: `void`
  open_file: (wallet_file) ->
    @rpc.request('wallet_open_file', ['wallet_file']).then (response) ->
      response.result

  # Opens the wallet of the given name
  # parameters: 
  #   wallet_name `wallet_name` - the name of the wallet to open
  # return_type: `void`
  open: (wallet_name) ->
    @rpc.request('wallet_open', ['wallet_name']).then (response) ->
      response.result

  # Creates a wallet with the given name
  # parameters: 
  #   wallet_name `wallet_name` - name of the wallet to create
  #   new_passphrase `new_passphrase` - a passphrase for encrypting the wallet
  # return_type: `void`
  create: (wallet_name, new_passphrase) ->
    @rpc.request('wallet_create', ['wallet_name', 'new_passphrase']).then (response) ->
      response.result

  # Returns the wallet name passed to wallet_open
  # parameters: 
  # return_type: `optional_wallet_name`
  get_name:  ->
    @rpc.request('wallet_get_name').then (response) ->
      response.result

  # Closes the curent wallet if one is open
  # parameters: 
  # return_type: `void`
  close:  ->
    @rpc.request('wallet_close').then (response) ->
      response.result

  # Exports the current wallet to a JSON file
  # parameters: 
  #   filename `json_filename` - the full path and filename of JSON file to generate, example: /path/to/exported_wallet.json
  # return_type: `void`
  export_to_json: (json_filename) ->
    @rpc.request('wallet_export_to_json', ['json_filename']).then (response) ->
      response.result

  # Creates a new wallet from an exported JSON file
  # parameters: 
  #   filename `json_filename` - the full path and filename of JSON wallet to import, example: /path/to/exported_wallet.json
  #   wallet_name `wallet_name` - name of the wallet to create
  # return_type: `void`
  create_from_json: (json_filename, wallet_name) ->
    @rpc.request('wallet_create_from_json', ['json_filename', 'wallet_name']).then (response) ->
      response.result

  # Lists all transactions for the specified account
  # parameters: 
  #   account_name `account_name` - the name of the account for which the transaction history will be returned, example: alice
  # return_type: `transaction_record_array`
  account_transaction_history: (account_name) ->
    @rpc.request('wallet_account_transaction_history', ['account_name']).then (response) ->
      response.result

  # Lock the private keys in wallet, disables spending commands until unlocked
  # parameters: 
  # return_type: `void`
  lock:  ->
    @rpc.request('wallet_lock').then (response) ->
      response.result

  # Unlock the private keys in the wallet to enable spending operations
  # parameters: 
  #   time_interval_in_seconds `timeout` - the number of seconds to keep the wallet unlocked
  #   passphrase `passphrase` - the passphrase for encrypting the wallet
  # return_type: `void`
  unlock: (timeout, passphrase) ->
    @rpc.request('wallet_unlock', ['timeout', 'passphrase']).then (response) ->
      response.result

  # Change the password of the current wallet
  # parameters: 
  #   passphrase `passphrase` - the passphrase for encrypting the wallet
  # return_type: `void`
  change_passphrase: (passphrase) ->
    @rpc.request('wallet_change_passphrase', ['passphrase']).then (response) ->
      response.result

  # Add new account for receiving payments
  # parameters: 
  #   account_name `account_name` - the name you will use to refer to this receive account
  # return_type: `public_key`
  create_account: (account_name) ->
    @rpc.request('wallet_create_account', ['account_name']).then (response) ->
      response.result

  # Add new account for sending payments
  # parameters: 
  #   account_name `account_name` - the name you will use to refer to this sending account
  #   public_key `account_key` - the key associated with this sending account
  # return_type: `void`
  add_contact_account: (account_name, account_key) ->
    @rpc.request('wallet_add_contact_account', ['account_name', 'account_key']).then (response) ->
      response.result

  # Return a pretty JSON representation of a transaction
  # parameters: 
  #   signed_transaction `transaction` - the transaction to pretty-print
  # return_type: `pretty_transaction`
  get_pretty_transaction: (transaction) ->
    @rpc.request('wallet_get_pretty_transaction', ['transaction']).then (response) ->
      response.result

  # 
  # parameters: 
  #   asset_symbol `symbol` - The type of account, ie: XTS, GLD, USD or * for everything
  #   receive_account_name `account_name` - The account to list the balance of.
  # return_type: `asset_array`
  get_balance: (symbol, account_name) ->
    @rpc.request('wallet_get_balance', ['symbol', 'account_name']).then (response) ->
      response.result

  # Sends given amount to the given address, assumes shares in DAC
  # parameters: 
  #   amount `amount_to_transfer` - the amount of shares to transfer
  #   asset_symbol `asset_symbol` - the asset to transfer
  #   sending_account_name `from_account_name` - the source account(s) to draw the shares from
  #   receive_account_name `to_account_name` - the account to transfer the shares to
  #   std::string `memo_message` - a memo to store with the transaction
  # return_type: `signed_transaction_array`
  transfer: (amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message) ->
    @rpc.request('wallet_transfer', ['amount_to_transfer', 'asset_symbol', 'from_account_name', 'to_account_name', 'memo_message']).then (response) ->
      response.result

  # Sends given amount to the given address, assumes shares in DAC
  # parameters: 
  #   int64_t `first_block_number` - the first block to scan
  #   int64_t `num_blocks` - the number of blocks to scan
  # return_type: `void`
  rescan_blockchain: (first_block_number, num_blocks) ->
    @rpc.request('wallet_rescan_blockchain', ['first_block_number', 'num_blocks']).then (response) ->
      response.result



angular.module("app").service("WalletAPI", ["$q", "$log", "RpcService", "$interval", WalletAPI])
