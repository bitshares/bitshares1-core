#  Generate a snapshot
#  Convert normal snapshot format to distribution list
#  Go through distribution list


import bitsharesrpc
import sys
import json
import os
import subprocess
import time

asset = sys.argv[1]

ORIGIN_PATH = "distributions/" + asset + "/distribution.json"
LIST_FORM_PATH = "distributions/" + asset + "/distribution-list.json"
NATIVE_FORM_PATH = "distributions/" + asset + "/distribution-native.json"

if True: # if need conversion
    with open( ORIGIN_PATH ) as distribution_file:
        distribution = json.load(distribution_file)
        distribution_list = []
        total = 0
        for addr, bal in distribution.iteritems():
            total += bal
            distribution_list.append([addr, bal])
        print total
        
        with open(LIST_FORM_PATH, 'w') as native_file:
            native_file.write(json.dumps(distribution_list, indent=4))
    subprocess.check_output(["../bts_convert_addresses", LIST_FORM_PATH, NATIVE_FORM_PATH])

rpc = bitsharesrpc.client("http://localhost:8000/rpc", "user", "password")

print "Checking status of distribution of blockchain"
rpc.blockchain_generate_issuance_map(asset, "/tmp/issuance_map.json")

issued = {}
total_issued = 0
with open("/tmp/issuance_map.json") as issued_file:
    issued_list = json.load(issued_file)
    for pair in issued_list:
        if pair[0] in issued:
            print "warning, duplicate issuances!"
            sys.exit(1)
        issued[pair[0]] = pair[1]
        total_issued += pair[1]

print "There are already %s issued balances totaling" % (str(len(issued)), total

with open(NATIVE_FORM_PATH) as native_file:
    distribution_list = json.load(native_file)
    distribution = {}
    for pair in distribution_list:
        if pair[0] not in issued:
            distribution[pair[0]] = pair[1]

    count = 0
    trx_set = []
    for addr, bal in distribution.iteritems():
        count += 1
        trx_set.append([addr, bal])
        if count == 100:
            print "Issuing 100 balances..."
            rpc.wallet_asset_issue_to_addresses(asset, trx_set)
            count = 0
            trx_set = []
            time.sleep(5)
    rpc.wallet_asset_issue_to_addresses(asset, trx_set)


# check result and compare
