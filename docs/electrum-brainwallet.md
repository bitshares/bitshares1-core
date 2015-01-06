1) Download electrum. Follow the instructions.

2) Make sure you know how to restore this wallet from the brain wallet seed.

3) Go to "receive" and copy one of the BTC addresses.  1jF4...

4) To send to this brain wallet:
    >>>  wallet_transfer_to_legacy_address 100 BTS my-account 1jF4...

5) To recover funds from this brain wallet:
In electrum console:
    > dumpprivkey("1jF4...")
    5afb...

In bitshares:

    >>> wallet_import_private_key 5afb... my-account

    >>> rescan

Now you should see your funds and can do a normal transfer.


BE CAREFUL!

*  Any funds in a legacy address will NOT be recoverable from master wallet key. Old backups will NOT have your funds.
*  Transferring funds from a legacy address MIGHT MOVE ALL THE FUNDS OUT OF THAT ADDRESS and send the change into a normal balance in your BTS wallet. Of course this change would be recoverable from wallet master private key. Manually transfer funds to the exactly where you want them to be safe.


