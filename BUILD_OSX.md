Building BitShares Toolkit on OS X 10.9
===============================

0) Install X Code by following these instructions https://guide.macports.org/chunked/installing.xcode.html

1) Download boost 1.54.0 from http://sourceforge.net/projects/boost/files/boost/1.54.0/
   
   Note: boost 1.55 does not compile with clang using the latest version of XCode as of (March 2014)

2) If you happen to have boost already installed and it was not compiled with clang or libc++ and c++11 then you will
 need to uninstall it prior to running the above steps.   

    sudo rm -r /usr/local/include/boost
    sudo rm -r /usr/local/lib/libboost*

3) Compile and install boost to support c++11 with clang using the following commands:

    tar -xjvf boost_1_54_0.tar.bz
    cd boost_1_54_0
    ./bootstrap
    sudo ./b2 toolset=clang cxxflags="-stdlib=libc++ std=c++11" linkflags="-stdlib=libc++" link=static install

4) Download OpenSSL  https://www.openssl.org/source/

    tar -xzvf openssl-1.0.1f.tar.gz
    cd openssl-1.0.1f
    ./Configure darwin64-x86_64-cc  
    make
    sudo make install

5) If you would like support for importing Bitcoin Core wallets then you will require Berkeley DB

    wget http://download.oracle.com/berkeley-db/db-6.0.30.tar.gz
    tar -xzvf db-6.0.30.tar.gz
    cd db-6.0.30/build_unix
    ../dist/configure --enable-cxx --prefix=/usr/local CPPFLAGS=-stdlib=libc++ 
    make
    sudo make install


6) Build BitShares Toolkit with CMake

    git submodule init
    git submodule update
    cmake -DCMAKE_PREFIX_PATH=/usr/local/ssl .
    make

