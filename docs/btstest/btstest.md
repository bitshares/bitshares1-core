

Introduction to btstest framework
---------------------------------

The purpose of this guide is to introduce other developers and community contributors to the `btstest`
testing framework by @theoreticalbts

Installing Python
-----------------

The `btstest` framework is implemented in Python 3.  Since the author uses Ubuntu LTS, it is most well-tested
with Python 3.4.0 on Ubuntu LTS, however some effort has been made to ease cross-platform compatibility.  In
particular, `btstest` does not use Python extensions; the Python standard library is sufficient.

Please note that Python 3 is *not* backwards compatible with Python 2.  Running `btstest` in Python 2 is not
supported and likely will not work!  However, it is easy to have concurrent installations of Python 2 and
Python 3.

See https://www.python.org/downloads/ for Windows, or your distribution's package manager on Linux.
On Ubuntu and other Debian-based distributions, Python 3 can be installed with the command:

    sudo apt-get install python3

Writing a simple test
---------------------

Let us write a simple test.  In this test, we will obtain some XTS, register a public account, and then
ensure the blockchain recognizes the account.  All tests are run on a private testnet created
specifically for the test using a custom genesis block.  Thus we will have access to private keys for
delegates and account holders.

First make a directory for the test:

    mkdir -p tests/btstests/tutorial/register_account

Creating testenv
----------------

We will create `testenv`.  The `testenv` file serves as the entry point for the test.
The main purpose of `testenv` is to act as Python code which sets up the environment for the test -- multiple clients, networking, credentials, etc.

Here are the steps:

- Create a test process `p_alice`
- Create an RPC endpoint `RPCClient` which will talk to the process over RPC
- Create a test client which contains test-specific functionality (save result of `execute_command_line` RPC into a buffer, then scan it using `expect_` methods)
- Register the client with the test framework using `register_client`
- Wait for RPC interface to activate
- Run all `.btstest` files in the current path `my_path`

Here is the code that achieves all of that:

    with _btstest.ClientProcess(name="alice") as p_alice:
        # create client process
        rpc_client = _btstest.RPCClient(dict(
            host="127.0.0.1",
            port=p_alice.http_port,
            rpc_user=p_alice.username,
            rpc_password=p_alice.password,
            ))
        test_client = _btstest.TestClient("alice", rpc_client)
        register_client(test_client)
        rpc_client.wait_for_rpc()
        run_testdir(my_path)

btstest file format
-------------------

The main purpose of `.btstest` files is to act as a list of commands and command output.
Taking inspiration from `regression_tests`, a session interacting with the BitShares client is a valid test!

Here is an example of a `.btstest` file with no output:

    >>> !client alice
    >>> !expect disable
    >>> debug_start_simulated_time "20140620T144030.000000"
    >>> wallet_create default password
    >>> wallet_set_automatic_backups false
    >>> debug_deterministic_private_keys 0 101 init true
    >>> wallet_delegate_set_block_production ALL true
    >>> wallet_set_transaction_scanning true
    >>> debug_wait_for_block_by_number 1
    >>> wallet_account_create myaccount
    >>> wallet_account_balance myaccount
    >>> debug_deterministic_private_keys 0 1 alice true myaccount false true
    >>> wallet_account_balance myaccount
    >>> blockchain_get_account myaccount
    >>> wallet_account_register myaccount myaccount
    >>> blockchain_get_account myaccount
    >>> blockchain_get_info
    >>> debug_advance_time 1 block
    >>> debug_wait_for_block_by_number 1 rlast
    >>> blockchain_get_info
    >>> blockchain_get_account myaccount
    >>> quit

However, the problem with copy-pasted output is that some output may change between testing and production, and some other output will change from run to run.


Improvements to btstest
-----------------------

- Document how to do comments in `.btstest` file
- Better whitespace handling
- Custom comment syntax
- When expected input doesn't match, display line number in `btstest` file
- Command line option to toggle echo on/off
- Have multiple `.btstest` in same directory do something meaningful
- Create `!include` command to allow common functionality to be split off
- Specify include path from command line or environment variable

Test API
--------

- `active_client` is a variable containing the name of the active client
- `expect_str` will expect command output matching a string
- `regex` or `expect_regex` will expect command output matching a regular expression
- `run_testdir` will run all `.btstest` files in given directory
- `register_client` will register a new `TestClient` object
- `_btstest` directly exposes the `btstest.py` module content.  The API should evolve to a point where using this is unnecessary

Goals of btstest framework
--------------------------

Here are the goals of `btstest`.  Note this is a preliminary version, not all these goals are achieved yet.

- Simple test format; a copy-pasted output log is a valid test!
- Robust value expectation.  With regular expressions, you can store an unknown value such as a txid.
- Templating.  With templating, you can expect a value stored by a regular expression.
- Simple framework installation.  Does not require libraries outside Python 3 standard library, so virtualenv should not be needed unless your Python configuration is highly unusual.
- Turing-complete helper scripts.  From short inline calls to pre-created functions, to complete Python 3 modules, a test can include Python code inline or in its testenv directory.
- Framework hooks.  Want to check macro / micro balance [1] after every command without cluttering your test script?  Just register it as a pre/post command hook in the local testenv!
- Want to run every unit test, checking macro / micro balance after every command?  Just register it as a global pre/post command hook in the global testenv!
- Want to use multiple clients?  Unified or separate logs are supported!
- Tired of verbose and error-prone block syncing commands?  There's a single built-in command to advance time for all clients and wait until all clients see the next block!
- Want to simulate delayed or dropped network connections?  Script this behavior using the built-in proxy!
- Custom genesis block?  No problem!
- Need to obtain numerical results to check calculations?  All commands' data is automatically received in JSON format and parsed!
- Want to test interactions of clients running multiple versions of the software?  Each client may have a different executable specified in the testenv!

[1] The code maintains certain global ("macro") statistics such as total shares in existence of BTS or BitAssets.  And of course data on individual positions ("micro").
The intent is to maintain an invariant condition where the macro statistic is always equal to the sum of the micro positions.  Checking macro / micro balance simply
means verifying this invariant by iterating through micro entities (e.g. all balances), and comparing the sum obtained to the macro statistic (which is supposed to
be equal to the sum obtained, but might not be if there is a bug where some transactions, chain rewinding logic or whatever doesn't properly update the macro
statistic).

