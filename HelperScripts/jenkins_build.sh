#!/bin/bash -xe
cd $WORKSPACE/bitsharesx
git submodule init
git submodule update
mkdir $WORKSPACE/build
cd $WORKSPACE/build
export BITSHARES_ROOT=$WORKSPACE
. ../bitsharesx/setenv.sh
cmake -DINCLUDE_QT_WALLET=TRUE -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE/toolchain.invictus/toolchain.invictus.cmake ../bitsharesx
make -j8 VERBOSE=1
