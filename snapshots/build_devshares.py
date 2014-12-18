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

with open("devshares.json", "w") as outfile:
    with open("ags-pts-dec-14.json") as infile:
        agspts = json.load(infile)
        for item in agspts["balances"]:
            new_genesis["balances"].append(item)
    with open("bts-dec-14.json") as infile:
        bts = json.load(infile)
        for item in bts:
            if item[1] == 0:
                continue
            new_genesis["balances"].append(item)

    outfile.write(json.dumps(new_genesis, indent=4))
