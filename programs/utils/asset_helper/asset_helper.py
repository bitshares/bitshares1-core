#  Generate a snapshot
#  Convert normal snapshot format to distribution list
#  Go through distribution list


import bitsharesrpc
import sys
import json
import os
import subprocess

asset = sys.argv[1]

if True: # if need conversion
    with open("distributions/" + asset + "/distribution.json") as distribution_file:
        distribution = json.load(distribution_file)
        distribution_list = []
        total = 0
        for addr, bal in distribution.iteritems():
            total += bal
            distribution_list.append([addr, bal])
        print total
        
        with open("distributions/" + asset + "/distribution-list.json", 'w') as native_file:
            native_file.write(json.dumps(distribution_list, indent=4))
    subprocess.check_output(["../bts_convert_addresses", "distributions/" + asset + "/distribution-list.json", "distributions/" + asset + "/distribution-native.json"])

rpc = bitsharesrpc.client("http://localhost:8000", "user", "password")

rpc.blockchain_generate_issuance_map(asset, "/tmp/issuance_map.json")

with open("/tmp/issuance_map.json") as issued_file:
    issued = json.load(issued_file)

# check if anything has been issued that shouldn't have been
# remove already-issued assets from list
# issue the rest

with open("distributions/" + asset + "/distribution-native.json") as native_file:
    distribution = json.load(native_file)
    count = 0
    trx_set = []
    for pair in distribution:
        count += 1
        trx_set.append(pair)
        if count == 100:
            rpc.wallet_asset_issue_to_addresses(asset, trx_set)
            count = 0
            trx_set = []
            time.sleep(5)
    rpc.wallet_asset_issue_to_addresses(asset, trx_set)


# check result and compare
