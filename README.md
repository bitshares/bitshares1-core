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
The BitShares source code is in the public domain under the Unlicense. See the [LICENSE file](https://github.com/BitShares/bitshares/blob/master/LICENSE.md) for more information.
