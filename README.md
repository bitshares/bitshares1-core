BitShares X [![Build Status](https://travis-ci.org/dacsunlimited/bitsharesx.png)](https://travis-ci.org/dacsunlimited/bitsharesx)
===============================
A DAC built with [BitShares Toolkit](https://github.com/BitShares/bitshares_toolkit)

Build Instructions
------------------
BitShares Toolkit uses git submodules for managing certain external dependencies. Before
you can build you will need to fetch the submodules with the following commands:

    git submodule init
    git submodule update
    cmake .
    make

Different platforms have different steps for handling dependencies, if you 
would like to build on OS X see BUILD_OSX.md

Documentation
------------------
Documentation is available at the GitHub wiki: https://github.com/BitShares/bitshares_toolkit/wiki.
