

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

Simple btstest usage
--------------------

The main purpose of `.btstest` files is to act as a list of commands and command output.
Taking inspiration from `regression_tests`, a session interacting with the BitShares client is a valid test!

Here is an example of a `.btstest` file with no output:

    >>> !client alice
    >>> !expect disable
    >>> debug_start_simulated_time "20140620T144030.000000"
    >>> wallet_create default "password"
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
    >>> debug_advance_time 1 block
    >>> debug_wait_for_block_by_number 1 rlast
    >>> blockchain_get_account myaccount
    >>> debug_advance_time 1 block
    >>> debug_wait_for_block_by_number 1 rlast
    >>> blockchain_get_account myaccount
    >>> quit

This test is present in the `tests/btstests/tutorial/register_account_0` directory with a suitable `testenv`.  The test can easily be run by compiling the `develop` branch and running the following command:

    tests/btstest.py tests/btstests/tutorial/register_account_0

The presence of `>>>` characters in a line flags that line as a command.  The rest of the line is passed through to the `execute_command_line` RPC, unless it is a *metacommand*, which
is a command that begins with `!`.  Metacommands are handled directly by the `btstest` framework.  In this case, the `!client` command addreses subsequent commands to the client named `alice` (which was specified in the testenv),
and the `!expect disable` command disables the *expectation functionality* (checking of command output).  Disabling expectation is useful to run a test as a *batch script*, only executing the commands and not complaining about failure to match output.  Which is often useful for developing new tests or upgrading badly broken tests.

Comments
--------

You can put a comment in a `.btstest` file as follows:

    #{ this is a comment }#

Expectation
-----------

Like `regression_tests`, the `btstest` framework has the capability to test that command output matches a given string.  Placing the previous test's output into a file and deleting `!expect disable` gives this output:

    >>> !showmatch enable
    >>> !client alice
    >>> debug_start_simulated_time "20140620T144030.000000"
    OK
    >>> wallet_create default "password"
    OK
    >>> wallet_set_automatic_backups false
    false
    >>> debug_deterministic_private_keys 0 101 init true
    [
      "5JZAeEjgszF9kXVpFsAK9wsVer2L3z9WwY63DT7p9kLNZUFYyRQ",
      "5KegApb8VGr1CAtUJH4a41KGqX1ZMK2ik64YCjRD3LR95k7LJAS",
      ...
      "5KZw3WpATeW1JdthN6jpdEAwJsgdupgF2CRovbjw4rCE9xvCZp7"
    ]
    >>> wallet_delegate_set_block_production ALL true
    OK
    >>> wallet_set_transaction_scanning true
    true
    >>> debug_wait_for_block_by_number 1
    OK
    >>> wallet_account_create myaccount
    "XTS8RCamk6kQtmQeaew1U42qaR67dFnSpQodEZUjWeQGQQhsm53PZ"
    ...
    >>> blockchain_get_account myaccount
    Name: myaccount
    Registered: 2014-06-20T14:40:30
    Last Updated: 36 weeks ago
    Owner Key: XTS8RCamk6kQtmQeaew1U42qaR67dFnSpQodEZUjWeQGQQhsm53PZ
    Active Key History:
    - XTS5VjXgz22FTUrPtv7Y2jp1HnWEcEQm2ksQLenRncBf59wSh6fmq, last used 36 weeks ago
    Not a delegate.
    >>> quit

Again, this file is present in `tests/btstests/tutorial/register_account_1` directory.  When you run this test, you will see the following output:

    >>> !client alice
    >>> debug_start_simulated_time "20140620T144030.000000"
    OK
    >>> wallet_create default "password"
    OK
    >>> wallet_set_automatic_backups false
    false
    >>> debug_deterministic_private_keys 0 101 init true
    [
      "5JZAeEjgszF9kXVpFsAK9wsVer2L3z9WwY63DT7p9kLNZUFYyRQ",
      ...
      "5KZw3WpATeW1JdthN6jpdEAwJsgdupgF2CRovbjw4rCE9xvCZp7"
    ]
    >>> wallet_delegate_set_block_production ALL true
    OK
    >>> wallet_set_transaction_scanning true
    true
    >>> debug_wait_for_block_by_number 1
    OK
    >>> wallet_account_create myaccount
    !{ "XTS8RCamk6kQtmQeaew1U42qaR67dFnSpQodEZUjWeQGQQhsm53PZ" }!~{ "XTS7wmhc95NpJuRQE8aGDUzzZhd7W34JQbgCbNdHrtPV1nnGxEq9V" }~
    ...
    >>> blockchain_get_account myaccount
    Name: myaccount
    Registered: 2014-06-20T14:40:30
    Last Updated: 36 weeks ago
    Owner Key: !{ XTS8RCamk6kQtmQeaew1U42qaR67dFnSpQodEZUjWeQGQQhsm53PZ }!~{ XTS7wmhc95NpJuRQE8aGDUzzZhd7W34JQbgCbNdHrtPV1nnGxEq9V }~
    Active Key History:
    - !{ XTS5VjXgz22FTUrPtv7Y2jp1HnWEcEQm2ksQLenRncBf59wSh6fmq, }!~{ XTS5kjEMtBUEzpoe2aZf4C8XGSZrTTc4Ch9Y1jdkp3XgnnA1qy2cE, }~ last used 36 weeks ago
    Not a delegate.
    >>> quit

The mismatches are clearly marked with `!{ }!` delimiters, and the text that was actually output is `~{ }~` delimiters.
It is clear that the mismatch is due to randomly generated keys which change from run to run.  In the next section, you will learn how to use regular expressions to deal with this properly and flexibly.

TODO:  Eventually mismatches will trigger a final error message and OS exit code, but this is not implemented yet.

TODO:  The `!showmatch` command will eventually allow to toggle between the above form and just straight-up output, but this is not implemented yet.

Expecting regular expressions
-----------------------------

The `${ }$` delimiters cause anything between to be interpreted as Python code.  The code runs in an environment which defines a few functions.  The one which will be presented in this section is called `expect_regex`.

Here is an example:

    >>> !showmatch enable
    >>> !client alice
    >>> debug_start_simulated_time "20140620T144030.000000"
    OK
    >>> wallet_create default "password"
    OK
    >>> wallet_set_automatic_backups false
    false
    >>> debug_deterministic_private_keys 0 101 init true
    [ ${ expect_regex(r'(?:\s*"[a-zA-Z0-9]+"[,]?){101}') }$ ]
    >>> wallet_delegate_set_block_production ALL true
    OK
    >>> wallet_set_transaction_scanning true
    true
    >>> debug_wait_for_block_by_number 1
    OK
    >>> wallet_account_create myaccount
    "XTS   ${ expect_regex(r'[a-zA-Z0-9]+') }$   "
    >>> wallet_account_balance myaccount
    No balances found.
    >>> debug_deterministic_private_keys 0 1 alice true myaccount false true
    [
      "${ expect_regex(r'[a-zA-Z0-9]+') }$"
    ]
    >>> wallet_account_balance myaccount
    ACCOUNT                         BALANCE                     
    ============================================================
    myaccount                       100,000.00000 XTS           
    >>> blockchain_get_account myaccount
    No account found.
    >>> wallet_account_register myaccount myaccount
    TIMESTAMP           BLOCK     FROM                TO                  AMOUNT                  MEMO                                        FEE                 ID      
    ======================================================================================================================================================================
    2014-06-20T14:40:30 PENDING   myaccount           myaccount           0.00000 XTS             register myaccount                          0.50000 XTS         ${ regex(r'[0-9a-f]{8}') }$
    >>> blockchain_get_account myaccount
    No account found.
    >>> debug_advance_time 1 block
    OK
    >>> debug_wait_for_block_by_number 1 rlast
    OK
    >>> blockchain_get_account myaccount
    No account found.
    >>> debug_advance_time 1 block
    OK
    >>> debug_wait_for_block_by_number 1 rlast
    OK
    >>> blockchain_get_account myaccount
    Name: myaccount
    Registered: 2014-06-20T14:40:30
    Last Updated: ${ expect_regex(r'[^\n]*') }$
    Owner Key: ${ expect_regex(r'[a-zA-Z0-9]+') }$
    Active Key History:
    - XTS ${ expect_regex(r'[a-zA-Z0-9]+') }$, last used ${ expect_regex(r'[^\n]*') }$
    Not a delegate.
    >>> quit

This test exists in `tests/btstests/tutorial/register_account_2`.

Here's a quick primer on what you need to know about Python's syntax and regular expression library in order to use `expect_regex`.
The regular expression passed to `expect_regex` is a Python string literal.
The `r` prefix denotes a *raw string literal* which is a Python language feature to disable the usual C-style handling of escape sequences for a particular string.
Using `r` allows regular expressions like the first one can contain sequences like `\s` which are passed through to the regular expression engine.
The actual syntax of the regular expressions themselves is detailed in `https://docs.python.org/3/library/re.html`.

This test also shows the `btstest` matching is rather "loose" about spare whitespace in your tests.
You can basically write whitespace anywhere in your test and it will be ignored.
This allows you to write readable tests by adding extra whitespace as needed.

You can see the regular expression match here:

    >>> blockchain_get_account myaccount
    Name: myaccount
    Registered: 2014-06-20T14:40:30
    Last Updated: ${ expect_regex(r'[^\n]*') }$~{36 weeks ago}~
    Owner Key: ${ expect_regex(r'[a-zA-Z0-9]+') }$~{XTS7A6gUbHCiye1JNv2HWNBxmjZxsUkPUXPCEeYLGcQk855umGi31}~
    Active Key History:
    - XTS ${ expect_regex(r'[a-zA-Z0-9]+') }$~{63EXgREEvRAFyzWw3apqTimmQwf3HvQALiMUYZzXXQwcza8Uf8}~, last used ${ expect_regex(r'[^\n]*') }$~{36 weeks ago}~
    Not a delegate.

TODO:  Regex flags will be passed in, but this is not currently implemented.  Will likely require killing the compiled RE cache.

Expecting JSON
--------------

The `expect_json()` function will expect a JSON object.  Currently no tests are performed on the object.

Expecting relative timestamps
-----------------------------

The `expect_reltime()` function will expect a relative time string as output by `fc::get_approximate_relative_time_string()`.

Expecting ISO timestamps
------------------------

The `expect_isotime()` function will expect an ISO format timestamp in the format `yyyy-mm-ddThh:mm:ss`

Improvements to btstest
-----------------------

- When expected input doesn't match, display line number in `btstest` file
- Command line option to toggle echo on/off
- Have multiple `.btstest` in same directory do something meaningful
- Create `!include` command to allow common functionality to be split off
- Specify include path from command line or environment variable
- REPL mode for directly talking to client(s)
- Instructions for using gdb
- Example(s) of tests that work with mainnet and testnet

Test API
--------

- `active_client` is a variable containing the name of the active client
- `expect_str` will expect command output matching a string
- `regex` or `expect_regex` will expect command output matching a regular expression
- `expect_json` expects a JSON object
- `expect_json` expects a relative time string as output by `fc::get_approximate_relative_time_string()`
- `run_testdir` will run all `.btstest` files in given directory
- `register_client` will register a new `TestClient` object
- `_btstest` directly exposes the `btstest.py` module content.  The API should evolve to a point where using this is unnecessary

TODO:  Document new features after refactoring

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

Using gdb with btstest
----------------------

Since `btstest` launches the BitShares executable, using GDB is non-trivial.  However, `btstest` has some support for this use case.

Include `debug_stop=True` in your `ClientProcess` creation in `testenv`, like so:

    with _btstest.ClientProcess(name="alice", debug_stop=True) as p_alice:

This passes the `--debug-stop` parameter to the process which causes the BitShares executable to issue a `SIGSTOP` to itself at a defined point in its startup.
Which will cause `btstest` to become unresponsive until you send the `SIGCONT` signal.
Then GDB may be attached with the `--pid` flag.  The process can be located by the PID file whose name is `alice.pid`:

    $ gdb --pid=$(cat tests/btstests/out/alice.pid)
    (gdb) continue
    (gdb) continue
    (gdb) continue

The `continue` command must be repeated several times on my system, I am not sure why.

The `debug_trap` RPC call may be used to break into GDB in the middle of a run.  For example, the following shows a script which might be used to debug a `wallet_transfer` command causing a crash:

    >>> balance alice
    ACCOUNT                         BALANCE   
    ============================================================
    alice                           199,999.50000 XTS
    >>> balance bob
    ACCOUNT                         BALANCE   
    ============================================================
    bob                             219,998.50000 XTS
                                    1,000.0000 USD
    >>> debug_trap
    >>> wallet_transfer 500 USD bob alice
