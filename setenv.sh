#!/bin/bash
pushd `dirname $0`/.. > /dev/null
: ${BITSHARES_ROOT:=`pwd -P`}
export BITSHARES_ROOT
popd > /dev/null
echo "Using: "$BITSHARES_ROOT "as BITSHARES_ROOT"

# needed for toolchain.invictus.cmake definitions
export INVICTUS_ROOT=$BITSHARES_ROOT

export TOOLCHAIN_ROOT=$BITSHARES_ROOT/toolchain.invictus
export PKG_CONFIG_SYSROOT_DIR=$TOOLCHAIN_ROOT/x86_64-unknown-linux-gnu/sysroot
export PKG_CONFIG_PATH=$TOOLCHAIN_ROOT/x86_64-unknown-linux-gnu/sysroot/usr/lib/pkgconfig

export QTDIR=$BITSHARES_ROOT/QT 
echo "Using: "$QTDIR "as QTDIR"
export ICUROOT=$BITSHARES_ROOT/ICU

export PATH=$WORKSPACE/build/bin:$QTDIR/bin:$WORKSPACE/build/programs/utils:$WORKSPACE/build/libraries/api:$PATH
export LD_LIBRARY_PATH=$ICUROOT/lib:$QTDIR/lib:$LD_LIBRARY_PATH

export OPENSSL_ROOT=$BITSHARES_ROOT/openssl
export OPENSSL_ROOT_DIR=$OPENSSL_ROOT
export OPENSSL_INCLUDE_DIR=$OPENSSL_ROOT/include

export DBROOTDIR=$BITSHARES_ROOT/BerkeleyDB
echo "Using: "$DBROOTDIR "as DBROOTDIR"

export BOOST_ROOT=$BITSHARES_ROOT/boost_1_55_0

