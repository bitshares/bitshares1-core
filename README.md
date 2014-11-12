BitShares Development Toolkit [![Build Status](https://travis-ci.org/vikramrajkumar/bitshares_toolkit.png)](https://travis-ci.org/vikramrajkumar/bitshares_toolkit)
===============================
The BitShares development toolkit is a set of libraries used to facilitate
the development of Decentralized Autonomous Companies (DACs).  It provides
a framework upon which new DACs can be developed based upon a common 
architecture.  

Build Instructions
------------------
BitShares Toolkit uses git submodules for managing certain external dependencies. Before
you can build you will need to fetch the submodules with the following commands:

    git submodule init
    git submodule update
    cmake .
    make

Different platforms have different steps for handling dependencies, check specific documents
for more details:

* [Ubuntu](https://github.com/janx/bitshares/blob/master/BUILD_UBUNTU.md)
* [OSX](https://github.com/janx/bitshares/blob/master/BUILD_OSX.md)
* [Windows](https://github.com/janx/bitshares/blob/master/BUILD_WIN32.md)

Documentation
------------------
Documentation is available at the GitHub wiki: https://github.com/BitShares/bitshares/wiki.
