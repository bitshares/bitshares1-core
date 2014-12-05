#!/bin/sh
#
# run testnet in tmux
#
set -e
set -x

rm -Rf tmp/0
rm -Rf tmp/1
rm -Rf tmp/2

mkdir -p tmp
[ -e tmp/genesis.json ] || tests/drltc_tests/tscript/genesis.py

tmux new-session -s testnet   -n node0 -d "programs/client/bitshares_client --data-dir tmp/0 --genesis-config tmp/genesis.json --min-delegate-connection-count 0 --server --rpcuser user --rpcpassword pass --upnp false --disable-default-peers --httpport 9100 --p2p-port r9200 --rpcport 9300 | tee tmp/0.out"
sleep 1
while ! ( tail -20 tmp/0.out | grep '^Listening for P2P connections on port' ) do
sleep 1
done
tmux new-window  -t testnet:1 -n node1    "programs/client/bitshares_client --data-dir tmp/1 --genesis-config tmp/genesis.json --min-delegate-connection-count 0 --server --rpcuser user --rpcpassword pass --upnp false --disable-default-peers --httpport 9101 --p2p-port r9201 --rpcport 9301 --connect-to 127.0.0.1:9200 | tee tmp/1.out"
tmux new-window  -t testnet:2 -n node2    "programs/client/bitshares_client --data-dir tmp/2 --genesis-config tmp/genesis.json --min-delegate-connection-count 0 --server --rpcuser user --rpcpassword pass --upnp false --disable-default-peers --httpport 9102 --p2p-port r9202 --rpcport 9302 --connect-to 127.0.0.1:9200 | tee tmp/2.out"

