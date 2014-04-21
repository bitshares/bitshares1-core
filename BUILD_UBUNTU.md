These instructions worked on a fresh Ubuntu 14.04 LTS image.

    sudo apt-get update
    sudo apt-get install git libreadline-dev uuid-dev g++ libdb++-dev libdb-dev zip libssl-dev openssl build-essential python-dev autotools-dev libicu-dev build-essential libbz2-dev libboost-dev libboost-all-dev
    git clone https://github.com/BitShares/bitshares_toolkit.git
    cd bitshares_toolkit
    git submodule init
    git submodule update
    cmake .
    make
