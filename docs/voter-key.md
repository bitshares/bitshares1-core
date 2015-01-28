This guide will walk you through the process of storing your funds in an offline
address while delegating the ability to update your vote to another key.

This is the simplest way to effectively secure your stake while still enabling you to vote.

This configuration is intended for large individual stakeholders and might not be the best
way to secure funds for different kinds of organizations. See guides on multi-sig and escrow
features.

Step 1) Create a hot wallet. Create an address for an account which you will have daily access to.
This will be your *voter address*.
```
>>> wallet_create online_wallet
>>> wallet_account_create voter-account
>>> wallet_address_create voter-account
BTS...voteraddress...
```

Create an offline wallet. Create an unregistered account and get an address for that account
This will be your *cold storage address*.
```
>>> wallet_create cold_storage
>>> wallet_account_create cold-account
>>> wallet_address_create cold-account
BTS...coldaddress...
```
It is easiest to keep all your funds in one balance. First collect everything into one account, then
in one transacton transfer funds from your hot wallet to your offline address.
```
>>> wallet_transfer_to_address 100 BTS my-acct BTS...coldaddress...
```
Get the balance ID for this address:
```
>>> blockchain_list_address_balances BTS...coldaddress...
BTS...balanceID
```
With your offline wallet, generate a transaction to delegate voting rights for that balance
owned by your cold storage address to your voter address.
```
>>> wallet_balance_set_vote_info BTS...balanceID... BTS...voteraddress... vote_all false "path/to/write/builder.trx"
```
Transfer and broadcast that transaction.
```
>>> wallet_builder_add_signature null true "path/to/builder.trx"
```

Now just manage your approved delegates and set your vote from the hot wallet:
```
>>> wallet_delegate_approve dev0.nikolai 1
>>> wallet_balance_set_vote_info BTS...balID...
```
Note that the balance ID will change each time because you use a new slate.
If you want to increase this particular balance, you have to send to the same address *with the same delegate slate*, otherwise
it will get a different balance ID and you will have to update the votes separately. It may be easiest to "vote_none" to clear the votes,
consolidate your balances, then update the vote all at once.


The voting key is allowed to withdraw a transaction fee once per hour, which means you can
use this key on its own to control the vote and do not need to tie it to other balances.


The rate limit protects you from a hacker draining your funds (assuming you pay attention),
but you should still be mindful of the fact that a compromised voting key makes 51% attacks marginally easier.


The voter key resets whenever you transfer the balance with the offline address.
