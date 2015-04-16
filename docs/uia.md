# BitShares User-Issued Asset Tutorial

This document is targeted at technical users already familiar with the basics of the BitShares platform and is intended to provide a hands-on demonstration of its
advanced user-issued asset feature. We will assume that the reader is familiar with downloading source code and compiling applications in a Linux or OSX terminal environment.

## Introduction

The BitShares platform provides a feature known as "user-issued assets", designed to help facilitate profitable business models for certain types of services that integrate with the platform.
The term user-issued asset refers to a type of custom token registered on the platform, and which users can hold and trade while obeying certain specified restrictions.
The creator--known as the issuer--of such an asset gets to publically name, describe, and distribute its tokens as desired. In addition, the issuer can specify
certain custom requirements for the asset: such as allowing only an approved whitelist of user accounts to hold the tokens, or requiring users to pay certain fees when transferring or trading the tokens.

For example, consider a currency exchange business that wants to use the BitShares trading engine--another feature of the platform--to build their trading service.
Such a business could accept deposits of real cash from verified customers
in exchange for user-issued asset tokens deposited to the customers' whitelisted BitShares accounts. The customers could then use the BitShares trading engine to trade these IOUs, while the issuer collects
a percentage of each trade as fees. When the customer is done trading and wants to cash out, the issuer pays them back real cash in exchange for the IOUs. In the end, the customer got the
trading service they wanted and the business collected trading fees, and leveraging the BitShares platform helped everyone keep things as efficient and profitable as possible.

## Getting Started

For this tutorial, we will be using the BitShares Test Network--called DevShares--for demonstrating usage of the user-issued asset system. In particular, we will be compiling and using
the command-line version of the DevShares client.

The DevShares source code can be found here: https://github.com/BitShares/DevShares

Build instructions can be found here:
 - https://github.com/BitShares/DevShares/blob/master/BUILD_OSX.md
 - https://github.com/BitShares/DevShares/blob/master/BUILD_UBUNTU.md

Follow the above instructions, and return to the root DevShares git repository folder after successfully building the client.

Now let's start the DevShares client:
```
ᐅ ./programs/client/devshares_client                                                                            Loading blockchain from: /Users/vikram/Library/Application Support/DevShares/chain
Creating default config file at: /Users/vikram/Library/Application Support/DevShares/config.json
Logging to file: /Users/vikram/Library/Application Support/DevShares/logs/default/default.log
Logging RPC to file: /Users/vikram/Library/Application Support/DevShares/logs/rpc/rpc.log
Logging blockchain to file: /Users/vikram/Library/Application Support/DevShares/logs/blockchain/blockchain.log
Logging P2P to file: /Users/vikram/Library/Application Support/DevShares/logs/p2p/p2p.log
Using built-in blockchain checkpoints
Initializing state from built-in genesis file
Please be patient, this will take several minutes...
Successfully replayed 0 blocks in 1 seconds.
Blockchain size changed from 0MiB to 0MiB.
Not starting RPC server, use --server to enable the RPC interface
(wallet closed) >>>
```

First, we will wait for the client to automatically synchronize with the network. You may see some status messages while this is occurring:
```
--- there are now 1 active connections to the p2p network
--- there are now 2 active connections to the p2p network
--- syncing with p2p network, 749094 blocks left to fetch
--- there are now 3 active connections to the p2p network
--- there are now 4 active connections to the p2p network
--- syncing with p2p network, our last block is 13 weeks old
--- syncing with p2p network, our last block is 82 days old
--- currently syncing at 2522 blocks/sec, 5 minutes remaining
```

Please be patient, as this may take some time. You can check the current status with the `info` command:
```
(wallet closed) >>> info
{
  "blockchain_head_block_num": 749196,
  "blockchain_head_block_age": "3 seconds old",
  "blockchain_head_block_timestamp": "2015-04-14T16:16:10",
  "blockchain_head_block_id": "21f7441bd6693247bac5bf3a5b14bbe01a51f8f2",
  "blockchain_average_delegate_participation": "96.19 %",
  "blockchain_share_supply": "2,804,787,065.64526 DVS",
  "blockchain_blocks_left_in_round": 22,
  "blockchain_next_round_time": "at least 4 minutes in the future",
  "blockchain_next_round_timestamp": "2015-04-14T16:19:50",
  "blockchain_random_seed": "6a298610c06bbb8325d129d695a61b0fc3484b51",
  "client_data_dir": "/Users/vikram/Library/Application Support/DevShares",
  "client_version": "0.9.0",
  "network_num_connections": 10,
  "network_num_connections_max": 200,
  "network_chain_downloader_running": false,
  "network_chain_downloader_blocks_remaining": null,
  "ntp_time": "2015-04-14T16:16:12",
  "ntp_time_error": -0.019751999999999999,
  "wallet_open": false,
  "wallet_unlocked": null,
  "wallet_unlocked_until": null,
  "wallet_unlocked_until_timestamp": null,
  "wallet_last_scanned_block_timestamp": null,
  "wallet_scan_progress": null,
  "wallet_block_production_enabled": null,
  "wallet_next_block_production_time": null,
  "wallet_next_block_production_timestamp": null
}
```

When `blockchain_head_block_age` is around 10 seconds or less, then you are done synchronizing with the network.

Note that commands can be listed/autocompleted using the Tab key, and that you can print the specification for each command using the special `help` command:
```
(wallet closed) >>> help help
Usage:
help [command_name]                                                                                   display a list of commands, or detailed help on an individual command
display a list of commands, or detailed help on an individual command

Parameters:
  command_name (method_name, optional, defaults to ""): the name of the method to get detailed help, or omit this for a list of commands

Returns:
  string

aliases: h
```
```
(wallet closed) >>> help info
Usage:
get_info                                                                                              Returns version number and associated information for this client
Returns version number and associated information for this client

Parameters:
  (none)

Returns:
  json_object

aliases: getinfo, info
```

## Demonstrating User-Issued Assets

First, we will create a new wallet:
```
(wallet closed) >>> create demo-wallet
new_passphrase:
new_passphrase (verify):
OK
demo-wallet (unlocked) >>>
```

The wallet will start off fully unlocked. If it automatically locks during the tutorial, you can use the `unlock` command to unlock it:
```
demo-wallet (locked) >>> unlock 3600
passphrase:
OK
demo-wallet (unlocked) >>>
```

Next, let's initialize two unregistered accounts:
```
demo-wallet (unlocked) >>> wallet_account_create demo-issuer
"DVS8Eu9kCSDqMVLsGmrb841wkF41rTbzKWAXxEKh85yLQY1YSMsJF"
```
```
demo-wallet (unlocked) >>> wallet_account_create demo-user
"DVS4yCcco9PT5kRwfEYh6UNQDYxFCxhe4QSK2tN5cPykES7YQD6fC"
```

Now we need to acquire some DevShares core tokens--called `DVS`--so that we can register these accounts and then register a user-issued asset. For the purposes of this tutorial, we will assume that some funds have been separately set aside beforehand and can be imported with the following command:
```
demo-wallet (unlocked) >>> wallet_import_private_key 5J2SXphZ7TqcZPjq4b5tCEdXhsTENKaji85L6G6uZGV7hTS5Pxn demo-issuer
Beginning scan at block 1...
Scan complete.
```

We can check our account balances to make sure the funds were imported successfully:
```
demo-wallet (unlocked) >>> balance
ACCOUNT                         BALANCE
============================================================
demo-issuer                     200,000.00000 DVS
```

Using these funds, we will publically register our account names:
```
demo-wallet (unlocked) >>> wallet_account_register demo-issuer demo-issuer
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T16:42:23 PENDING   demo-issuer         demo-issuer         0.00000 DVS             register demo-issuer                        1.00000 DVS         94d22f4f
```
```
demo-wallet (unlocked) >>> wallet_account_register demo-user demo-issuer
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T16:42:34 PENDING   demo-issuer         demo-user           0.00000 DVS             register demo-user                          1.00000 DVS         818c9402
```

We can make sure these registrations were successful:
```
demo-wallet (unlocked) >>> history
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T16:42:23 749349    demo-issuer         demo-issuer         0.00000 DVS             register demo-issuer                        1.00000 DVS         94d22f4f
2015-04-14T16:42:34 749350    demo-issuer         demo-user           0.00000 DVS             register demo-user                          1.00000 DVS         818c9402
```
```
demo-wallet (unlocked) >>> wallet_list_accounts
NAME (* delegate)                  KEY                                                             REGISTERED            BLOCK PRODUCTION ENABLED
demo-issuer                        DVS5uBmE1hQjKCZSAGQJY6BGDu1yRFAD7nBADFpk55jWRqNViXDqy           2015-04-14T16:42:20   N/A
demo-user                          DVS6APHZftbKhVoNSCnpHKbZk5vHV2xzF3kgYxooYwEx9SYLKUViB           2015-04-14T16:42:30   N/A
```

Now we are ready to create and register a new user-issued asset:
```
demo-wallet (unlocked) >>> help wallet_uia_create
Usage:
wallet_uia_create <issuer_account> <symbol> <name> <description> <max_supply_with_trailing_decimals>   Create a new user-issued asset on the blockchain. Warning: creation fees can be very high!
Create a new user-issued asset on the blockchain. Warning: creation fees can be very high!

Parameters:
  issuer_account (string, required): The registered account name that will pay the creation fee and control the new asset
  symbol (asset_symbol, required): A unique symbol that will represent the new asset. Short symbols are very expensive!
  name (string, required): A human-readable name for the new asset
  description (string, required): A human-readable description of the new asset
  max_supply_with_trailing_decimals (string, required): Choose the max share supply and max share divisibility for the new asset. For example: 10000000000.00000 or 12345.6789

Returns:
  transaction_record
```
```
demo-wallet (unlocked) >>> wallet_uia_create demo-issuer DEMOIOU "Demo IOU" "This IOU is for demonstration purposes" 25000.00
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:05:43 PENDING   demo-issuer         demo-issuer         0.00000 DVS             create DEMOIOU (Demo IOU)                   5,000.00000 DVS     36da50ac
```

Confirm that the registration was successful:
```
demo-wallet (unlocked) >>> history
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T16:42:23 749349    demo-issuer         demo-issuer         0.00000 DVS             register demo-issuer                        1.00000 DVS         94d22f4f
2015-04-14T16:42:34 749350    demo-issuer         demo-user           0.00000 DVS             register demo-user                          1.00000 DVS         818c9402
2015-04-14T17:05:43 749483    demo-issuer         demo-issuer         0.00000 DVS             create DEMOIOU (Demo IOU)                   5,000.00000 DVS     36da50ac
```
```
demo-wallet (unlocked) >>> blockchain_get_asset DEMOIOU
{
  "id": 48,
  "symbol": "DEMOIOU",
  "issuer_id": 581,
  "authority": {
    "required": 1,
    "owners": [
      "DVSNwn2ZDRhT12U3jKM9t9xVHbfiQDvyUPeZ"
    ]
  },
  "authority_flag_permissions": 4294967295,
  "active_flags": 0,
  "whitelist": [],
  "name": "Demo IOU",
  "description": "This IOU is for demonstration purposes",
  "public_data": null,
  "precision": 100,
  "max_supply": 2500000,
  "withdrawal_fee": 0,
  "market_fee_rate": 0,
  "collected_fees": 0,
  "current_supply": 0,
  "registration_date": "2015-04-14T17:05:40",
  "last_update": "2015-04-14T17:05:40"
}
```

Note that there are many commands for administrating user-issued assets:
```
wallet_uia_collect_fees              
wallet_uia_issue                         
wallet_uia_retract_balance               
wallet_uia_update_authority_permissions  
wallet_uia_update_fees                   
wallet_uia_update_whitelist
wallet_uia_create                        
wallet_uia_issue_to_addresses            
wallet_uia_update_active_flags           
wallet_uia_update_description            
wallet_uia_update_supply
```

For this tutorial, we will demonstrate just a couple user-issued asset customization features: whitelisted accounts and market trading fees. We will start with enabling the restricted accounts setting on the asset:
```
demo-wallet (unlocked) >>> help wallet_uia_update_active_flags
Usage:
wallet_uia_update_active_flags <paying_account> <asset_symbol> <flag> <enable_instead_of_disable>     Activate or deactivate one of the special flags for the specified user-issued asset as permitted
Activate or deactivate one of the special flags for the specified user-issued asset as permitted

Parameters:
  paying_account (account_name, required): the account that will pay the transaction fee
  asset_symbol (asset_symbol, required): the user-issued asset to update
  flag (asset_flag_enum, required): the special flag to enable or disable; one of {dynamic_max_supply, dynamic_fees, halted_markets, halted_withdrawals, retractable_balances, restricted_accounts}
  enable_instead_of_disable (bool, required): true to enable, or false to disable

Returns:
  transaction_record
```
```
demo-wallet (unlocked) >>> wallet_uia_update_active_flags demo-issuer DEMOIOU restricted_accounts true
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:14:33 PENDING   demo-issuer         UNKNOWN             0.00000 DVS             add restricted_accounts to authority per... 1.00000 DVS         97652098
```

The account whitelist starts off empty. Let's try issuing some of the asset tokens to a user account:
```
demo-wallet (unlocked) >>> help wallet_uia_issue
Usage:
wallet_uia_issue <asset_amount> <asset_symbol> <recipient> [memo_message]                             Issue shares of a user-issued asset to the specified recipient
Issue shares of a user-issued asset to the specified recipient

Parameters:
  asset_amount (string, required): the amount of shares of the asset to issue
  asset_symbol (asset_symbol, required): specify the unique symbol of the asset
  recipient (string, required): the account name, public key, address, btc address, or contact label (prefixed by "label:") which will receive the funds
  memo_message (string, optional, defaults to ""): a memo to send if the recipient is an account

Returns:
  transaction_record
```
```
demo-wallet (unlocked) >>> wallet_uia_issue 10 DEMOIOU demo-user
10 assert_exception: Assert Exception
asset_rec->address_is_approved( *eval_state.pending_state(), owner ):
```

The issuance has failed since the recipient account is not in the whitelist. Let's add it to the whitelist and try again:
```
demo-wallet (unlocked) >>> help wallet_uia_update_whitelist
Usage:
wallet_uia_update_whitelist <paying_account> <asset_symbol> <account_name> <add_to_whitelist>         Add or remove the specified registered account from the specified user-issued asset's whitelist
Add or remove the specified registered account from the specified user-issued asset's whitelist

Parameters:
  paying_account (account_name, required): the account that will pay the transaction fee
  asset_symbol (asset_symbol, required): the user-issued asset that will have its whitelist updated
  account_name (string, required): the name of the account to add or remove from the whitelist
  add_to_whitelist (bool, required): true to add to whitelist, or false to remove

Returns:
  transaction_record
```
```
demo-wallet (unlocked) >>> wallet_uia_update_whitelist demo-issuer DEMOIOU demo-user true
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:18:54 PENDING   demo-issuer         UNKNOWN             0.00000 DVS             add demo-user towhitelist for DEMOIOU       1.00000 DVS         8b50d6cb
```
```
demo-wallet (unlocked) >>> wallet_uia_issue 10 DEMOIOU demo-user
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:19:12 PENDING   demo-issuer         demo-user           10.00 DEMOIOU                                                       1.00000 DVS         a3ed410f
```
```
demo-wallet (unlocked) >>> balance
ACCOUNT                         BALANCE
============================================================
demo-issuer                     194,995.00000 DVS
demo-user                       10.00 DEMOIOU
demo-wallet (unlocked) >>>
```

The asset tokens have now been issued successfully to the whitelisted account!

Next let's see if we can collect some market trading fees. We will enable the dynamic fees setting for the asset, then set a 1% market trading fee:
```
demo-wallet (unlocked) >>> wallet_uia_update_active_flags demo-issuer DEMOIOU dynamic_fees true
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:29:12 PENDING   demo-issuer         UNKNOWN             0.00000 DVS             add dynamic_fees to authority permsfor D... 1.00000 DVS         43c74ca6
```
```
demo-wallet (unlocked) >>> help wallet_uia_update_fees
Usage:
wallet_uia_update_fees <paying_account> <asset_symbol> [withdrawal_fee] [market_fee_rate]             Update the transaction fee, market fee rate for the specified user-issued asset if permitted
Update the transaction fee, market fee rate for the specified user-issued asset if permitted

Parameters:
  paying_account (account_name, required): the account that will pay the transaction fee
  asset_symbol (asset_symbol, required): the user-issued asset to update
  withdrawal_fee (string, optional, defaults to ""): the transaction fee for the asset in shares of the asset
  market_fee_rate (string, optional, defaults to ""): the market fee rate for the asset as a percentage between 0.01 and 100, or 0

Returns:
  transaction_record
```
```
demo-wallet (unlocked) >>> wallet_uia_update_fees demo-issuer DEMOIOU "" 1
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:29:28 PENDING   demo-issuer         UNKNOWN             0.00000 DVS             update properties for DEMOIOU               1.00000 DVS         8d02feed
demo-wallet (unlocked) >>>
```

Now let's put the tokens up for sale on the internal trading platform (after sending `demo-user` some `DVS` so he can pay the network fee for posting the offer):
```
demo-wallet (unlocked) >>> transfer 10 DVS demo-issuer demo-user
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:36:19 PENDING   demo-issuer         demo-user           10.00000 DVS                                                        1.00000 DVS         4841bafc
```
```
demo-wallet (unlocked) >>> balance
ACCOUNT                         BALANCE
============================================================
demo-issuer                     194,982.00000 DVS
demo-user                       10.00000 DVS
                                10.00 DEMOIOU
```
```
demo-wallet (unlocked) >>> help bid
Usage:
wallet_market_submit_bid <from_account_name> <quantity> <quantity_symbol> <base_price> <base_symbol> [allow_stupid_bid]   Used to place a request to buy a quantity of assets at a price specified in another asset
Used to place a request to buy a quantity of assets at a price specified in another asset

Parameters:
  from_account_name (account_name, required): the account that will provide funds for the bid
  quantity (string, required): the quantity of items you would like to buy
  quantity_symbol (asset_symbol, required): the type of items you would like to buy
  base_price (string, required): the price you would like to pay
  base_symbol (asset_symbol, required): the type of asset you would like to pay with
  allow_stupid_bid (bool, optional, defaults to "false"): Allow user to place bid at more than 5% above the current sell price.

Returns:
  transaction_record

aliases: bid
```
```
demo-wallet (unlocked) >>> bid demo-user 100 DVS 0.1 DEMOIOU
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:39:24 PENDING   demo-user           demo-user           10.00 DEMOIOU           buy DVS @ 0.1 DEMOIOU / DVS                 1.00000 DVS         61be881e
```
```
demo-wallet (unlocked) >>> blockchain_market_order_book DEMOIOU DVS
                             BIDS (* Short)                                  |                                   ASKS
TOTAL                     QUANTITY                                     PRICE | PRICE                                        QUANTITY                     TOTAL   COLLATERAL
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
10.00 DEMOIOU             100.00000 DVS                          0.1 DEMOIOU |
```

As shown above, we've put up an offer to trade `10 DEMOIOU` for `100 DVS`. Let's match that offer:
```
demo-wallet (unlocked) >>> help ask
Usage:
wallet_market_submit_ask <from_account_name> <sell_quantity> <sell_quantity_symbol> <ask_price> <ask_price_symbol> [allow_stupid_ask]   Used to place a request to sell a quantity of assets at a price specified in another asset
Used to place a request to sell a quantity of assets at a price specified in another asset

Parameters:
  from_account_name (account_name, required): the account that will provide funds for the ask
  sell_quantity (string, required): the quantity of items you would like to sell
  sell_quantity_symbol (asset_symbol, required): the type of items you would like to sell
  ask_price (string, required): the price per unit sold.
  ask_price_symbol (asset_symbol, required): the type of asset you would like to be paid
  allow_stupid_ask (bool, optional, defaults to "false"): Allow user to place ask at more than 5% below the current buy price.

Returns:
  transaction_record

aliases: ask
```
```
demo-wallet (unlocked) >>> ask demo-issuer 100 DVS 0.1 DEMOIOU
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:44:22 PENDING   demo-issuer         demo-issuer         100.00000 DVS           sell DVS @ 0.1 DEMOIOU / DVS                1.00000 DVS         42becc88
```
```
demo-wallet (unlocked) >>> history
 TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
========================================================================================================================================================================
 2015-04-14T16:42:23 749349    demo-issuer         demo-issuer         0.00000 DVS             register demo-issuer                        1.00000 DVS         94d22f4f
 2015-04-14T16:42:34 749350    demo-issuer         demo-user           0.00000 DVS             register demo-user                          1.00000 DVS         818c9402
 2015-04-14T17:05:43 749483    demo-issuer         demo-issuer         0.00000 DVS             create DEMOIOU (Demo IOU)                   5,000.00000 DVS     36da50ac
 2015-04-14T17:14:33 749533    demo-issuer         UNKNOWN             0.00000 DVS             add restricted_accounts to authority per... 1.00000 DVS         97652098
 2015-04-14T17:18:54 749558    demo-issuer         UNKNOWN             0.00000 DVS             add demo-user towhitelist for DEMOIOU       1.00000 DVS         8b50d6cb
 2015-04-14T17:19:12 749560    demo-issuer         demo-user           10.00 DEMOIOU                                                       1.00000 DVS         a3ed410f
 2015-04-14T17:29:12 749618    demo-issuer         UNKNOWN             0.00000 DVS             add dynamic_fees to authority permsfor D... 1.00000 DVS         43c74ca6
 2015-04-14T17:29:28 749619    demo-issuer         UNKNOWN             0.00000 DVS             update properties for DEMOIOU               1.00000 DVS         8d02feed
 2015-04-14T17:36:19 749659    demo-issuer         demo-user           10.00000 DVS                                                        1.00000 DVS         4841bafc
 2015-04-14T17:39:24 749677    demo-user           BID-d7015edd        10.00 DEMOIOU           buy DVS @ 0.1 DEMOIOU / DVS                 1.00000 DVS         61be881e
 2015-04-14T17:44:22 749705    demo-issuer         ASK-76aaa279        100.00000 DVS           sell DVS @ 0.1 DEMOIOU / DVS                1.00000 DVS         42becc88
------------------------------------------------------------------------------------------------------------------------------------------------------------------------
|2015-04-14T17:51:40 749747    BID-d7015edd        MARKET              10.00 DEMOIOU           pay bid @ 0.1 DEMOIOU / DVS                 0.00000 DVS         VIRTUAL |
|                              BID-d7015edd        demo-user           100.00000 DVS           bid proceeds @ 0.1 DEMOIOU / DVS                                        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------
|2015-04-14T17:51:40 749747    ASK-76aaa279        MARKET              100.00000 DVS           fill ask @ 0.1 DEMOIOU / DVS                0.00000 DVS         VIRTUAL |
|                              ASK-76aaa279        demo-issuer         9.90 DEMOIOU            ask proceeds @ 0.1 DEMOIOU / DVS                                        |
------------------------------------------------------------------------------------------------------------------------------------------------------------------------
```
```
demo-wallet (unlocked) >>> balance
ACCOUNT                         BALANCE
============================================================
demo-issuer                     194,678.00000 DVS
                                9.90 DEMOIOU
demo-user                       109.00000 DVS
demo-wallet (unlocked) >>>
```

We can see that 1% of the `10 DEMOIOU` that was traded is missing, and we can verify that is has indeed been collected as fees:
```
demo-wallet (unlocked) >>> blockchain_get_asset DEMOIOU
{
  "id": 48,
  "symbol": "DEMOIOU",
  
  [...]
  
  "collected_fees": 10,
  "current_supply": 1000,
  "registration_date": "2015-04-14T17:05:40",
  "last_update": "2015-04-14T17:51:00"
}
```

Finally, the issuer is able to reclaim these collected fees:
```
demo-wallet (unlocked) >>> wallet_uia_collect_fees DEMOIOU demo-issuer
TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID
======================================================================================================================================================================
2015-04-14T17:59:21 PENDING   demo-issuer         demo-issuer         0.10 DEMOIOU                                                        1.00000 DVS         f41ae67b
```
```
demo-wallet (unlocked) >>> balance
ACCOUNT                         BALANCE
============================================================
demo-issuer                     194,677.00000 DVS
                                10.00 DEMOIOU
demo-user                       109.00000 DVS
```

And we're done!

The above was a basic demonstration of a few features of user-issued assets on the BitShares platform, and has hopefully illustrated some of their power and potential for enabling or enhancing certain types of services and business models.

## Connecting Services to the BitShares Platform

The preferred way to integrate external services with the BitShares platform is via the JSON-RPC protocol, which allows access to all the same commands available using the above command-line interface.

Basic instructions can be found at these links:
- https://github.com/BitShares/bitshares#using-the-rpc-server
- https://github.com/xeroc/python-bitsharesrpc
