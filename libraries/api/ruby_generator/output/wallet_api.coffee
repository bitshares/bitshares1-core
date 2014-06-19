# Warning: this is a generated file, any changes made here will be overwritten by the build process

class WalletAPI

  constructor: (@q, @log, @rpc, @interval) ->
    #@log.info "---- WalletAPI Constructor ----"


  # Extra information about the wallet.
  # parameters: 
  # return_type: `json_object`
  get_info:  ->
    @rpc.request('wallet_get_info').then (response) ->
      response.result

  # Opens the wallet at the given path
  # parameters: 
  #   filename `wallet_file` - the full path and filename of the wallet to open, example: /path/to/wallet.dat
  # return_type: `void`
  open_file: (wallet_file) ->
    @rpc.request('wallet_open_file', [wallet_file]).then (response) ->
      response.result

  # Opens the wallet of the given name
  # parameters: 
  #   wallet_name `wallet_name` - the name of the wallet to open
  # return_type: `void`
  open: (wallet_name) ->
    @rpc.request('wallet_open', [wallet_name]).then (response) ->
      response.result

  # Creates a wallet with the given name
  # parameters: 
  #   wallet_name `wallet_name` - name of the wallet to create
  #   new_passphrase `new_passphrase` - a passphrase for encrypting the wallet
  #   new_passphrase `brain_key` - a strong passphrase that will be used to generate all private keys, defaults to a large random number
  # return_type: `void`
  create: (wallet_name, new_passphrase, brain_key) ->
    @rpc.request('wallet_create', [wallet_name, new_passphrase, brain_key]).then (response) ->
      response.result

  # Returns the wallet name passed to wallet_open
  # parameters: 
  # return_type: `optional_wallet_name`
  get_name:  ->
    @rpc.request('wallet_get_name').then (response) ->
      response.result

  # Loads the private key into the specified account
  # parameters: 
  #   wif_private_key `wif_key` - A private key in bitcoin Wallet Import Format (WIF)
  #   account_name `account_name` - the name of the account the key should be imported into, if null then the key must belong to an active account
  #   bool `create_new_account` - If true, the wallet will attempt to create a new account for the name provided rather than import the key into an existing account
  #   bool `rescan` - If true, the wallet will rescan the blockchain looking for transactions that involve this private key
  # return_type: `void`
  import_private_key: (wif_key, account_name, create_new_account, rescan) ->
    @rpc.request('wallet_import_private_key', [wif_key, account_name, create_new_account, rescan]).then (response) ->
      response.result

  # Imports a bitcoin wallet
  # parameters: 
  #   filename `wallet_filename` - the path to the bitcoin wallet you would like to import.
  #   passphrase `passphrase` - the bitcoin wallet's password, not your bitshares wallet password.
  #   account_name `account_name` - the account name to receive the contents of the wallet, you must create this account first if it does not exist.
  # return_type: `void`
  import_bitcoin: (wallet_filename, passphrase, account_name) ->
    @rpc.request('wallet_import_bitcoin', [wallet_filename, passphrase, account_name]).then (response) ->
      response.result

  # Imports a armory wallet
  # parameters: 
  #   filename `wallet_filename` - the armory wallet
  #   passphrase `passphrase` - the wallet's password
  #   account_name `account_name` - the account name to receive the contents of the wallet
  # return_type: `void`
  import_armory: (wallet_filename, passphrase, account_name) ->
    @rpc.request('wallet_import_armory', [wallet_filename, passphrase, account_name]).then (response) ->
      response.result

  # Imports a electrum wallet
  # parameters: 
  #   filename `wallet_filename` - the electrum wallet
  #   passphrase `passphrase` - the wallet's password
  #   account_name `account_name` - the account name to receive the contents of the wallet
  # return_type: `void`
  import_electrum: (wallet_filename, passphrase, account_name) ->
    @rpc.request('wallet_import_electrum', [wallet_filename, passphrase, account_name]).then (response) ->
      response.result

  # Imports a multibit wallet
  # parameters: 
  #   filename `wallet_filename` - the multibit wallet
  #   passphrase `passphrase` - the wallet's password
  #   account_name `account_name` - the account name to receive the contents of the wallet
  # return_type: `void`
  import_multibit: (wallet_filename, passphrase, account_name) ->
    @rpc.request('wallet_import_multibit', [wallet_filename, passphrase, account_name]).then (response) ->
      response.result

  # Create the key from keyhotee config and import it to the wallet, creating a new account using this key
  # parameters: 
  #   name `firstname` - first name in keyhotee profile config, for salting the seed of private key
  #   name `middlename` - middle name in keyhotee profile config, for salting the seed of private key
  #   name `lastname` - last name in keyhotee profile config, for salting the seed of private key
  #   brainkey `brainkey` - brainkey in keyhotee profile config, for salting the seed of private key
  #   keyhoteeid `keyhoteeid` - using keyhotee id as account name
  # return_type: `void`
  import_keyhotee: (firstname, middlename, lastname, brainkey, keyhoteeid) ->
    @rpc.request('wallet_import_keyhotee', [firstname, middlename, lastname, brainkey, keyhoteeid]).then (response) ->
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
    @rpc.request('wallet_export_to_json', [json_filename]).then (response) ->
      response.result

  # Creates a new wallet from an exported JSON file
  # parameters: 
  #   filename `json_filename` - the full path and filename of JSON wallet to import, example: /path/to/exported_wallet.json
  #   wallet_name `wallet_name` - name of the wallet to create
  #   passphrase `imported_wallet_passphrase` - passphrase of the imported wallet
  # return_type: `void`
  create_from_json: (json_filename, wallet_name, imported_wallet_passphrase) ->
    @rpc.request('wallet_create_from_json', [json_filename, wallet_name, imported_wallet_passphrase]).then (response) ->
      response.result

  # Lists all transactions for the specified account
  # parameters: 
  #   account_name `account_name` - the name of the account for which the transaction history will be returned, example: alice
  # return_type: `pretty_transactions`
  account_transaction_history: (account_name) ->
    @rpc.request('wallet_account_transaction_history', [account_name]).then (response) ->
      response.result

  # Clear "stuck" pending transactions from the wallet.
  # parameters: 
  # return_type: `void`
  clear_pending_transactions:  ->
    @rpc.request('wallet_clear_pending_transactions').then (response) ->
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
    @rpc.request('wallet_unlock', [timeout, passphrase]).then (response) ->
      response.result

  # Change the password of the current wallet
  # parameters: 
  #   passphrase `passphrase` - the passphrase for encrypting the wallet
  # return_type: `void`
  change_passphrase: (passphrase) ->
    @rpc.request('wallet_change_passphrase', [passphrase]).then (response) ->
      response.result

  # Return a list of wallets in the current data directory
  # parameters: 
  # return_type: `wallet_name_array`
  list:  ->
    @rpc.request('wallet_list').then (response) ->
      response.result

  # Add new account for receiving payments
  # parameters: 
  #   account_name `account_name` - the name you will use to refer to this receive account
  #   json_variant `private_data` - Extra data to store with this account record
  # return_type: `public_key`
  account_create: (account_name, private_data) ->
    @rpc.request('wallet_account_create', [account_name, private_data]).then (response) ->
      response.result

  # Updates the trust placed in a given delegate
  # parameters: 
  #   account_name `delegate_name` - the name of the delegate to set trust level on
  #   int32_t `trust_level` - Positive for Trust, Negative for Distrust, 0 for Neutral
  # return_type: `void`
  set_delegate_trust_level: (delegate_name, trust_level) ->
    @rpc.request('wallet_set_delegate_trust_level', [delegate_name, trust_level]).then (response) ->
      response.result

  # Add new account for sending payments
  # parameters: 
  #   account_name `account_name` - the name you will use to refer to this sending account
  #   public_key `account_key` - the key associated with this sending account
  # return_type: `void`
  add_contact_account: (account_name, account_key) ->
    @rpc.request('wallet_add_contact_account', [account_name, account_key]).then (response) ->
      response.result

  # Return a pretty JSON representation of a transaction
  # parameters: 
  #   signed_transaction `transaction` - the transaction to pretty-print
  # return_type: `pretty_transaction`
  get_pretty_transaction: (transaction) ->
    @rpc.request('wallet_get_pretty_transaction', [transaction]).then (response) ->
      response.result

  # Sends given amount to the given address, assumes shares in DAC.  This transfer will occur using multiple transactions as necessary to maximize your privacy, but will be more costly.
  # parameters: 
  #   real_amount `amount_to_transfer` - the amount of shares to transfer, will be multiplied by the precision associated by asset_symbol
  #   asset_symbol `asset_symbol` - the asset to transfer
  #   sending_account_name `from_account_name` - the source account(s) to draw the shares from
  #   receive_account_name `to_account_name` - the account to transfer the shares to
  #   string `memo_message` - a memo to store with the transaction
  # return_type: `signed_transaction_array`
  multipart_transfer: (amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message) ->
    @rpc.request('wallet_multipart_transfer', [amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message]).then (response) ->
      response.result

  # Sends given amount to the given address, assumes shares in DAC.  This transfer will occur in a single transaction and will be cheaper, but may reduce your privacy.
  # parameters: 
  #   real_amount `amount_to_transfer` - the amount of shares to transfer
  #   asset_symbol `asset_symbol` - the asset to transfer
  #   sending_account_name `from_account_name` - the source account(s) to draw the shares from
  #   receive_account_name `to_account_name` - the account to transfer the shares to
  #   string `memo_message` - a memo to store with the transaction
  # return_type: `signed_transaction`
  transfer: (amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message) ->
    @rpc.request('wallet_transfer', [amount_to_transfer, asset_symbol, from_account_name, to_account_name, memo_message]).then (response) ->
      response.result

  # Scans the transaction history for operations relevant to this wallet.
  # parameters: 
  #   uint32_t `first_block_number` - the first block to scan
  #   uint32_t `num_blocks` - the number of blocks to scan
  # return_type: `void`
  rescan_blockchain: (first_block_number, num_blocks) ->
    @rpc.request('wallet_rescan_blockchain', [first_block_number, num_blocks]).then (response) ->
      response.result

  # Updates the data published about a given account
  # parameters: 
  #   account_name `account_name` - the account that will be updated
  #   account_name `pay_from_account` - the account from which fees will be paid
  #   json_variant `public_data` - public data about the account
  #   bool `as_delegate` - true if account_name should be upgraded to a delegate.
  # return_type: `transaction_record`
  account_register: (account_name, pay_from_account, public_data, as_delegate) ->
    @rpc.request('wallet_account_register', [account_name, pay_from_account, public_data, as_delegate]).then (response) ->
      response.result

  # Updates the data published about a given account
  # parameters: 
  #   account_name `account_name` - the account that will be updated
  #   account_name `pay_from_account` - the account from which fees will be paid
  #   json_variant `public_data` - public data about the account
  #   bool `as_delegate` - true if account_name should be upgraded to a delegate.
  # return_type: `transaction_record`
  account_update_registration: (account_name, pay_from_account, public_data, as_delegate) ->
    @rpc.request('wallet_account_update_registration', [account_name, pay_from_account, public_data, as_delegate]).then (response) ->
      response.result

  # Lists all foreign addresses and their labels associated with this wallet
  # parameters: 
  # return_type: `wallet_account_record_array`
  list_contact_accounts:  ->
    @rpc.request('wallet_list_contact_accounts').then (response) ->
      response.result

  # TODO: wrong description? Lists all foreign addresses and their labels associated with this wallet
  # parameters: 
  # return_type: `wallet_account_record_array`
  list_receive_accounts:  ->
    @rpc.request('wallet_list_receive_accounts').then (response) ->
      response.result

  # TODO: wrong description? Lists all foreign addresses and their labels associated with this wallet
  # parameters: 
  #   account_name `account_name` - the name of the account to retrieve
  # return_type: `wallet_account_record`
  get_account: (account_name) ->
    @rpc.request('wallet_get_account', [account_name]).then (response) ->
      response.result

  # Remove a contact account from your wallet
  # parameters: 
  #   account_name `account_name` - the name of the contact
  # return_type: `void`
  remove_contact_account: (account_name) ->
    @rpc.request('wallet_remove_contact_account', [account_name]).then (response) ->
      response.result

  # Rename an account in wallet
  # parameters: 
  #   account_name `current_account_name` - the current name of the account
  #   new_account_name `new_account_name` - the new name for the account
  # return_type: `void`
  account_rename: (current_account_name, new_account_name) ->
    @rpc.request('wallet_account_rename', [current_account_name, new_account_name]).then (response) ->
      response.result

  # Creates a new user issued asset
  # parameters: 
  #   asset_symbol `symbol` - the ticker symbol for the new asset
  #   string `asset_name` - the name of the asset
  #   string `issuer_name` - the name of the issuer of the asset
  #   string `description` - a description of the asset
  #   json_variant `data` - arbitrary data attached to the asset
  #   int64_t `maximum_share_supply` - the maximum number of shares of the asset
  #   int64_t `precision` - defines where the decimal should be displayed, must be a power of 10
  # return_type: `signed_transaction`
  asset_create: (symbol, asset_name, issuer_name, description, data, maximum_share_supply, precision) ->
    @rpc.request('wallet_asset_create', [symbol, asset_name, issuer_name, description, data, maximum_share_supply, precision]).then (response) ->
      response.result

  # Issues new shares of a given asset type
  # parameters: 
  #   real_amount `amount` - the amount of shares to issue
  #   asset_symbol `symbol` - the ticker symbol for asset
  #   account_name `to_account_name` - the name of the account to receive the shares
  #   string `memo_message` - the memo to send to the receiver
  # return_type: `signed_transaction`
  asset_issue: (amount, symbol, to_account_name, memo_message) ->
    @rpc.request('wallet_asset_issue', [amount, symbol, to_account_name, memo_message]).then (response) ->
      response.result

  # Submit a proposal to the delegates for voting
  # parameters: 
  #   account_name `delegate_account_name` - the delegate account proposing the issue
  #   string `subject` - the subject of the proposal
  #   string `body` - the body of the proposal
  #   string `proposal_type` - the type of the proposal
  #   json_variant `data` - the name of the account to receive the shares
  # return_type: `signed_transaction`
  submit_proposal: (delegate_account_name, subject, body, proposal_type, data) ->
    @rpc.request('wallet_submit_proposal', [delegate_account_name, subject, body, proposal_type, data]).then (response) ->
      response.result

  # Vote on a proposal
  # parameters: 
  #   account_name `name` - the name of the account to vote with
  #   proposal_id `proposal_id` - the id of the proposal to vote on
  #   vote_type `vote` - your vote
  #   string `message` - comment to go along with vote
  # return_type: `signed_transaction`
  vote_proposal: (name, proposal_id, vote, message) ->
    @rpc.request('wallet_vote_proposal', [name, proposal_id, vote, message]).then (response) ->
      response.result

  # Lists the unspent balances that are controlled by this wallet
  # parameters: 
  #   account_name `account_name` - the account for which unspent balances should be listed
  #   asset_symbol `symbol` - The symbol to filter by, '*' for all
  # return_type: `wallet_balance_record_array`
  list_unspent_balances: (account_name, symbol) ->
    @rpc.request('wallet_list_unspent_balances', [account_name, symbol]).then (response) ->
      response.result

  # Lists the total balances of all accounts sorted by account and asset
  # parameters: 
  #   account_name `account_name` - the account to get a balance for, '*' or ''.  If '*' or '' then all accounts will be returned
  # return_type: `map< account_name, map< asset_symbol, share_type> >`
  account_balance: (account_name) ->
    @rpc.request('wallet_account_balance', [account_name]).then (response) ->
      response.result

  # Lists all public keys in this account
  # parameters: 
  #   account_name `account_name` - the account for which public keys should be listed
  # return_type: `public_key_summary_array`
  account_list_public_keys: (account_name) ->
    @rpc.request('wallet_account_list_public_keys', [account_name]).then (response) ->
      response.result

  # Used to transfer some of the delegates pay from their balance
  # parameters: 
  #   account_name `delegate_name` - the delegate who's pay is being cashed out
  #   account_name `to_account_name` - the account that should receive the funds
  #   real_amount `amount_to_withdraw` - the amount to withdraw
  #   string `memo` - memo to add to transaction
  # return_type: `signed_transaction`
  withdraw_delegate_pay: (delegate_name, to_account_name, amount_to_withdraw, memo) ->
    @rpc.request('wallet_withdraw_delegate_pay', [delegate_name, to_account_name, amount_to_withdraw, memo]).then (response) ->
      response.result

  # Used to set the priority fee for new transactions. Return current fee if no parameter is provided.
  # parameters: 
  #   amount `fee` - the prioty fee to be setted.
  # return_type: `void`
  set_priority_fee: (fee) ->
    @rpc.request('wallet_set_priority_fee', [fee]).then (response) ->
      response.result

  # Used to place a request to buy a quantity of assets at a price specified in another asset
  # parameters: 
  #   account_name `from_account_name` - the account that will provide funds for the bid
  #   real_amount `quantity` - the quantity of items you would like to buy
  #   asset_symbol `quantity_symbol` - the type of items you would like to buy
  #   real_amount `quote_price` - the price you would like to pay
  #   asset_symbol `quote_symbol` - the type of asset you would like to pay with
  # return_type: `signed_transaction`
  market_submit_bid: (from_account_name, quantity, quantity_symbol, quote_price, quote_symbol) ->
    @rpc.request('wallet_market_submit_bid', [from_account_name, quantity, quantity_symbol, quote_price, quote_symbol]).then (response) ->
      response.result

  # List an order list of a specific market
  # parameters: 
  #   asset_symbol `quote_symbol` - the quote symbol of the market
  #   asset_symbol `base_symbol` - the base symbol of the market
  #   int64_t `limit` - the maximum number of items to return
  # return_type: `market_order_status_array`
  market_order_list: (quote_symbol, base_symbol, limit) ->
    @rpc.request('wallet_market_order_list', [quote_symbol, base_symbol, limit]).then (response) ->
      response.result

  # Cancel an order
  # parameters: 
  #   address `order_id` - the address of the order to cancel
  # return_type: `signed_transaction`
  market_cancel_order: (order_id) ->
    @rpc.request('wallet_market_cancel_order', [order_id]).then (response) ->
      response.result

  # Reveals the private key corresponding to an address or public key
  # parameters: 
  #   string `address_or_public_key` - a public key or address (based on hash of public key) of the private key
  # return_type: `string`
  dump_private_key: (address_or_public_key) ->
    @rpc.request('wallet_dump_private_key', [address_or_public_key]).then (response) ->
      response.result

  # Returns the allocation of votes by this account
  # parameters: 
  #   account_name `account_name` - the account to report votes on, or empty for all accounts
  # return_type: `account_vote_summary`
  account_vote_summary: (account_name) ->
    @rpc.request('wallet_account_vote_summary', [account_name]).then (response) ->
      response.result

  # returns the private key for this account in WIF format
  # parameters: 
  #   account_name `account_name` - the name of the account key to export
  # return_type: `string`
  account_export_private_key: (account_name) ->
    @rpc.request('wallet_account_export_private_key', [account_name]).then (response) ->
      response.result

  # Set a property in the GUI settings DB
  # parameters: 
  #   string `name` - the name of the setting to set
  #   variant `value` - the value to set the setting to
  # return_type: `void`
  set_setting: (name, value) ->
    @rpc.request('wallet_set_wallet_setting', [name, value]).then (response) ->
      response.result

  # Get the value of the given setting
  # parameters: 
  #   string `name` - The name of the setting to fetch
  # return_type: `optional_variant`
  get_setting: (name) ->
    @rpc.request('wallet_get_wallet_setting', [name]).then (response) ->
      response.result

  # Enable or disable block production for a particular delegate account
  # parameters: 
  #   string `delegate_name` - The delegate to enable/disable block production for
  #   bool `enable` - True to enable block production, false otherwise
  # return_type: `void`
  enable_delegate_block_production: (delegate_name, enable) ->
    @rpc.request('wallet_enable_delegate_block_production', [delegate_name, enable]).then (response) ->
      response.result



angular.module("app").service("WalletAPI", ["$q", "$log", "RpcService", "$interval", WalletAPI])
