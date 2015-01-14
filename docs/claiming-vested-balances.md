
In version 0.5.0 we unlocked the vesting balances from the sharedrop.

Unlocking funds as fast as possible means that it is not very usable, but the important thing is that you have access to the funds.


You can check your original vesting balances in the console:

    >>> wallet_check_sharedrop

    [{
        "original_address": "..."
        "original_balance": 123
      },{
        "original_address": "..."
        "original_balance": 123
      },{
        "original_address": "..."
        "original_balance": 123
      }
    ]


To claim what has vested so far:

    >>> wallet_collect_vested_balances my-account


Please note that this will group all your vested balances into one transaction. If you are concerned about privacy, you could manually import keys one by one into distinct wallets.
Later versions of the wallet will make it easy to see how much has vested and how much you can claim. If you need this information now, it is available by inspecting the result of `blockchain_get_balance`.
