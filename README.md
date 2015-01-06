BitShares
=========
BitShares is a software platform designed to help coordinate voluntary free market operations amongst a set of social actors.

These social actors together maintain a replicated deterministic state machine which defines the state of a free market. This state machine unambigiously defines the ownership of resources amongst market participants, the rules by which resources are reallocated through market operations, and the history of all market operations. Social actors are free to voluntarily enter and exit the market as desired.

Replicas of the state machine are kept consistent using the [Delegated Proof-of-Stake](http://wiki.bitshares.org/index.php/DPOS_or_Delegated_Proof_of_Stake) distributed consensus protocol, which depends on market operations by a special class of market participants colloquially known as shareholders. Resource ownership is secured using digital signatures and inputs to the state machine are shared amongst actors using a peer-to-peer mesh network.

Features
--------
The system is designed to ensure the following properties:
- Fault-Tolerance: the market should be resilient to bad actors
- Immutability: the historical intent of all market participants should be preserved
- Transparency: any actor can inspect the market to verify that it is operating correctly
- Censorship Resistance: no actor can be kept from performing valid market operations
- Flexibility: the rules of the market should be able to change given sufficient shareholder approval
- Self-Sustainability: the market should be be able to fund its own continued operation

Additional information is available at [BitShares.org](http://bitshares.org/) and the [BitShares Wiki](http://wiki.bitshares.org/index.php/Main_Page). Community discussion occurs at [BitSharesTalk.org](https://bitsharestalk.org/).

Building
--------
Different platforms have different build instructions:
* [OS X](https://github.com/BitShares/bitshares/blob/master/BUILD_OSX.md)
* [Ubuntu](https://github.com/BitShares/bitshares/blob/master/BUILD_UBUNTU.md)
* [Windows](https://github.com/BitShares/bitshares/blob/master/BUILD_WIN32.md)


Using the RPC server
--------------------

For many applications, it is useful to execute BitShares commands from scripts.  The BitShares client includes RPC server functionality to allow programs to submit JSON-formatted commands and retrieve JSON-formatted results over an HTTP connection.  To enable the RPC server, you can edit the `rpc` section of `config.json` as follows:

      "rpc": {
        "enable": true,
        "rpc_user": "USERNAME",
        "rpc_password": "PASSWORD",
        "rpc_endpoint": "127.0.0.1:1775",
        "httpd_endpoint": "127.0.0.1:1776",

Here, `USERNAME` and `PASSWORD` are authentication credentials which must be presented by a client to gain access to the RPC interface.  These parameters may also be specified on the command line, but this is not recommended because some popular multi-user operating systems (Linux in particular) allow command line parameters of running programs to be visible to all users.

After editing the configuration file and (re)starting the BitShares client, you can use any HTTP client to POST a JSON object and read the JSON response.  Here is an example using the popular `curl` command line HTTP client:

    curl --user USERNAME:PASSWORD http://127.0.0.1:1776/rpc -X POST -H 'Content-Type: application/json' -d '{"method" : "blockchain_get_account", "params" : ["dev0.theoretical"], "id" : 1}'

The POST request returns a JSON result like this (some data elided for brevity):

    {"id":1,"result":{"id":31427,"name":"dev0.theoretical","public_data":{"version":"v0.4.27.1"},"owner_key":"BTS75vj8aaDWFwg7Wd6WinAAqVddUcSRJ1hSMDNayLAbCuxsmoQTf", ...},"meta_data":{"type":"public_account","data":""}}}

Since HTTP basic authentication is used, the authentication credentials are sent over the socket in unencrypted plaintext.
For this reason, binding to an interface other than localhost in the configuration file is not recommended.
If you wish to access the RPC interface from a remote system, you should establish a secure connection using SSH port forwarding (the `-L` option in OpenSSH) or a reverse proxy SSL/TLS tunnel (typically supported by general-purpose webservers such as `nginx`).

Please keep in mind that anyone able to connect to the RPC socket with the correct username and password will be able to access all funds, accounts and private keys in any open wallet (including wallets opened manually or by another RPC client connected to the same `bitshares_client` instance).
Thus, your security procedures should protect the username, password, and socket accordingly (including `config.json` since it contains the username and password)!

Contributing
------------
The source code can always be found at the [BitShares GitHub Repository](https://github.com/BitShares/bitshares). There are four main branches:
- `master` - official BitShares releases are tagged from here; this should only change for a new release
- `bitshares` - updates to BitShares are staged here in preparation for the next official release
- `develop` - all new development happens here; this is what is used for internal BitShares XTS test networks
- `toolkit` - this is the most recent common ancestor between master and develop; forks of BitShares should base from here

Some technical documentation is available at the [BitShares GitHub Wiki](https://github.com/BitShares/bitshares/wiki).

Support
-------
Bugs can be reported directly to the [BitShares Issue Tracker](https://github.com/BitShares/bitshares/issues).

Technical support can be obtained from the [BitSharesTalk Technical Support Forum](https://bitsharestalk.org/index.php?board=45.0).

License
-------
The BitShares source code is in the public domain under the Unlicense. See the [LICENSE](https://github.com/BitShares/bitshares/blob/master/LICENSE.txt) for more information.
