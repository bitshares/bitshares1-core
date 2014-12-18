import pprint
import json
import datetime
import subprocess

new_genesis = {}
new_genesis["timestamp"] = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
new_genesis["names"] = []

with open("devshare-delegates.txt", "w") as dels:
    for i in range(101):
        keys = json.loads(subprocess.check_output(["../programs/utils/bts_create_key"]))
        dels.write(">>> wallet_import_private_key " + keys["wif_private_key"] + "\n")

        acct = {
            "name": "init" + str(i),
            "owner": keys["public_key"],
            "delegate_pay_rate": 1
        }
        new_genesis["names"].append(acct)

new_genesis["balances"] = []
new_genesis["bts_sharedrop"] = []

with open("devshares.json", "w") as outfile:
    with open("ags-pts-dec-14.json") as infile:
        agspts = json.load(infile)
        agspts_total = 0
        for item in agspts["balances"]:
            if item[1] == 0:
                continue
            new_genesis["balances"].append(item)
            agspts_total += item[1]
        print "total agspts balance: " + str(agspts_total)

    with open("bts-dec-14.json") as bts_snapshot_json:
        with open("../libraries/blockchain/genesis_bts.json") as bts_genesis_json:
            snap = json.load(bts_snapshot_json)
            vest = json.load(bts_genesis_json)
            total = 0
            snap_total = 0
            vest_total = 0
            for item in snap:
                total += item[1]
                snap_total += item[1]
            for item in vest["bts_sharedrop"]:
                total += item["balance"]
                vest_total += item["balance"]

            for item in snap:
                if item[1] == 0:
                    continue
                balance = (100000000000000.0 * (1. * snap_total / total)) * (1. * item[1] / snap_total)
                new_genesis["balances"].append([item[0], int(balance)])

            for item in vest["bts_sharedrop"]:
                if item["balance"] == 0:
                    continue

                balance = (100000000000000.0 * (1. * vest_total / total)) * (1. * item["balance"] / vest_total)
                new_genesis["bts_sharedrop"].append({
                    "raw_address": item["raw_address"],
                    "balance": int(balance)
                })

        print "total normal BTS balance: " + str(snap_total)
        print "total vesting BTS balance: " + str(vest_total)
                
    outfile.write(json.dumps(new_genesis, indent=4))
