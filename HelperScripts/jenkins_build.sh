#!/bin/bash -xe
cd $WORKSPACE/bitshares
git submodule init
git submodule update
mkdir $WORKSPACE/build
cd $WORKSPACE/build
export BITSHARES_ROOT=$WORKSPACE
. ../bitshares/setenv.sh
cmake -DINCLUDE_QT_WALLET=TRUE -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE/toolchain.invictus/toolchain.invictus.cmake ../bitshares
make -j8 VERBOSE=1
