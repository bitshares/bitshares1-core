class Wallet

  constructor: (@q, @log, @rpc, @error_service, @interval) ->
    @log.info "---- Wallet Constructor ----"


  # Opens the wallet at the given path
  open_file: (wallet_file) ->
    @rpc.request('wallet_open_file', ['wallet_file']).then (response) ->
      response.result

  # Opens the wallet of the given name
  open: (wallet_name) ->
    @rpc.request('wallet_open', ['wallet_name']).then (response) ->
      response.result

  # Creates a wallet with the given name
  create: (wallet_name, passphrase) ->
    @rpc.request('wallet_create', ['wallet_name', 'passphrase']).then (response) ->
      response.result

  # Returns the wallet name passed to wallet_open
  get_name:  ->
    @rpc.request('wallet_get_name').then (response) ->
      response.result

  # Closes the curent wallet if one is open
  close:  ->
    @rpc.request('wallet_close').then (response) ->
      response.result

  # Exports the current wallet to a JSON file
  export_to_json: (json_filename) ->
    @rpc.request('wallet_export_to_json', ['json_filename']).then (response) ->
      response.result

  # Creates a new wallet from an exported JSON file
  create_from_json: (json_filename, wallet_name) ->
    @rpc.request('wallet_create_from_json', ['json_filename', 'wallet_name']).then (response) ->
      response.result

  # Lock the private keys in wallet, disables spending commands until unlocked
  lock:  ->
    @rpc.request('wallet_lock').then (response) ->
      response.result

  # Unlock the private keys in the wallet to enable spending operations
  unlock: (timeout, passphrase) ->
    @rpc.request('wallet_unlock', ['timeout', 'passphrase']).then (response) ->
      response.result

  # Change the password of the current wallet
  change_passphrase: (passphrase) ->
    @rpc.request('wallet_change_passphrase', ['passphrase']).then (response) ->
      response.result

  # Add new account for receiving payments
  create_account: (account_name) ->
    @rpc.request('wallet_create_account', ['account_name']).then (response) ->
      response.result

  # Add new account for sending payments
  add_contact_account: (account_name, account_key) ->
    @rpc.request('wallet_add_contact_account', ['account_name', 'account_key']).then (response) ->
      response.result

  # Return a pretty JSON representation of a transaction
  get_pretty_transaction: (transaction) ->
    @rpc.request('wallet_get_pretty_transaction', ['transaction']).then (response) ->
      response.result

  # Sends given amount to the given address, assumes shares in DAC
  transfer: (amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message) ->
    @rpc.request('wallet_transfer', ['amount_to_transfer', 'asset_symbol', 'from_account_name', 'to_account_name', 'memo_message']).then (response) ->
      response.result



angular.module("app").service("WalletAPI", ["$q", "$log", "RpcService", "ErrorService", "$interval", Wallet])
