Building BitShares on OS X 10.9
===============================

0) Install X Code by following these instructions https://guide.macports.org/chunked/installing.xcode.html
1) Download boost 1.55.0 from http://sourceforge.net/projects/boost/files/boost/1.55.0/
2) Compile and install boost to support c++11 with clang using the following commands:

    tar -xjvf boost_1_55_0.tar.bz
    cd boost_1_55_0
    ./bootstrap
    sudo ./b2 toolset=clang cxxflags="-stdlib=libc++" linkflags="-stdlib=libc++" link=static install

