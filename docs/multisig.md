This will show you how to make use of the multisig feature using the command line.


First, have each party to the multisig create a normal address.

    >>> wallet_address_create alice
    BTS...alice-address...

    >>> wallet_address_create bob
    BTS...bob-address...

    >>> wallet_address_create carol
    BTS...carol-address...


Now lets deposit from account 'angel' to a 2-of-3 multisig with the above users. Notice the last two arguments
are the required number of signatures and the allowed addresses.

    >>> wallet_multisig_deposit 100 BTS alice 2 [BTS...alice-addres..., BTS...bob-address..., BTS...carol-address...]

That's it! Making a 2-of-2 or whatever other combination you like is done the same way.

This is a good time to make note of the multisig balance ID for this deposit, as you will need to know
this to withdraw.
The first argument is the asset type for this multisig ID.

    >>> wallet_multisig_get_balance_id BTS 2 [BTS...alice-addres..., BTS...bob-address..., BTS...carol-address...]
    BTS...multisigID...

Withdrawing is trickier because it requires you to pass around a partial transaction. Fortunately the bitshares client
makes this relatively simple by automatically writing the latest transaction builder into your data directory.
Note that I have to withdraw to an address, as the client doesn't support withdrawing to an account yet.

    >>> wallet_multisig_withdraw_start 50 BTS BTS...multisigID... BTS...alice-address...

You will see a big JSON dump, this is the transaction builder. Go into your "wallets" directory and you should see
a folder called "trx". Inside there should be a file called "latest.trx" which has the same JSON data you just saw.
This is a transaction builder which has all the signatures your wallet was able to add.

Send this file to another potential signer. He can add a signature like so:

    >>> wallet_builder_file_add_signature true "path/to/builder_file/latest.trx"

(The first argument is whether to try to broadcast the transaction or not)

Now you will see a "latest.trx" in the wallets/trx folder, hopefully with yet another signature. Repeat until you
have enough signatures and you should see the transaction successfully broadcasted.



If you are doing this process via an RPC wrapper, you can use the returned transaction builder from "wallet_multisig_withdraw_start"
and pass it to "wallet_builder_add_signature" to avoid having to work with the builder files. This is inconvenient to do from the CLI
because you would have to escape a huge JSON string.
