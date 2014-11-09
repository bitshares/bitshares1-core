# BTS merger allocation draft 1

#  ags-pts-nov-5.json
#  662e2bd091a401108fd9736dd556a2c8744111e1  -
#  vote-aug-21-noFMV.json
#  c9a08d1a6a1921b1caf2c793cc77f62a2056db1f  -
#  fmv-key.json
#  b5b55c27b50f6c7fe9737e16b0c438dcd0da7ec4  -
#  dns-dev-fund.json
#  0847a39bd69e1cc7dc073317184bb789bf8d89f2  -
#  dns-collapse.json
#  2e2aa23930ec8e2cbca4e9cf7ea14daa620fb1a1  -

import json

# Collect (address, balance) pairs
exodus_balances = []

# All assets have precision 10**8

# These should add up to 500m * 10**8
bts_added = 0
bts_exluded = 0

# AGS and PTS - generated using snapshot tool. No substitutions.
with open("ags-pts-nov-5.json") as agspts:
    snapshot = json.load(agspts)
    agspts_total = 0
    for item in snapshot["balances"]:
        if item[0] == "KEYHOTEE_FOUNDERS":
            bts_exluded += int(item[1])
            continue
        agspts_total += int(item[1])
        exodus_balances.append(item)
        bts_added += int(item[1])

# VOTE
with open("vote-aug-21-noFMV.json") as vote:
    snapshot = json.load(vote)
    vote_total = 0
    for item in snapshot["balances"]:
        if item[0] == "KEYHOTEE_FOUNDERS":
            #print "removing keyhotee ags: " + str(item[1])
            bts_exluded += int(item[1])
            continue
        vote_total += int(item[1])
        exodus_balances.append(item)
        bts_added += int(item[1])
    #print "**  vote-aug-21-noFMV.json    total:  " + str(vote_total)

# Follow my vote dev stake
with open("fmv-key.json") as fmvkey:
    item = json.load(fmvkey)[0]
    exodus_balances.append(item)
    bts_added += item[1]
    print "FMV allocation: " + str(item[1])


pangel_dns = 0 # PaNGEL address
dns_no_dev = 0
dns_scaled_total = 0
bts_to_bonus = 0

# DNS normal - remove pangel and dev funds
with open("dns-dev-fund.json") as devfund:
    devkeys = json.load(devfund)
    with open("dns-nov-5.json") as dns:
        snapshot = json.load(dns)
        for item in snapshot:
            if (item[0] in devkeys and item[1] != 0):
                if item[1] < 200000000 * 10**8:
                    #pangel_dns += item[1]
                    continue
                else:
                    #print "skipping dev balance: " + str(item[1])
                    continue
            if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
                pangel_dns += item[1]
            dns_no_dev += item[1]

        #print "dns no dev: " + str(dns_no_dev)
        scale = 75000000.0 / dns_no_dev
        scale *= 10 ** 8
        print "BTS per DNS: " + str(scale / 1000)

        for item in snapshot:
            if (item[0] in devkeys and item[1] != 0):
                #print "skipping dev balance: " + str(item[1])
                continue
            if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
                continue
            balance = int(scale * int(item[1]))
            bts_added += balance
            exodus_balances.append([item[0], balance])

        bts_to_bonus = int(scale * pangel_dns) 

print "bts to bonus: " + str(bts_to_bonus / (10**8))

# DNS extra - add pangel funds and exchange subsidy
with open("dns-collapse.json") as collapse:
    collapse = json.load(collapse)
    collapse_total = 0
    for item in collapse:
        if (item[0] in devkeys and item[1] != 0):
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            continue
        collapse_total += int(item[1])
  
    bts_for_exchanges = bts_to_bonus * 2/3 # 10000000 * 10**8
    print "bts for exchanges: " + str(bts_for_exchanges / 10**8)
    exodus_balances.append(["PZ6KoPSkGD3dqiFzoueTmreAxWBCgvRKQ5", bts_for_exchanges])
    bts_added += bts_for_exchanges
    scale = 1.0 * (bts_to_bonus - bts_for_exchanges) / collapse_total
    for item in collapse:
        if item[0] in devkeys:
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            continue

        balance = int(scale * int(item[1]))
        exodus_balances.append([item[0], balance])
        bts_added += balance


print "bts_added: " + str(bts_added)
print "bts_excluded: " + str(bts_exluded)
print "total reviewed: " + str(bts_added + bts_exluded)

real_total = 0
output = []
for item in exodus_balances:
    if item[1] != 0:
        real_total += item[1]
        obj = {
            "raw_address": item[0],
            "balance": item[1]
        }
        output.append(obj)

print "Real total: " + str(real_total / 10**8)

with open("libraries/blockchain/bts-sharedrop.json", "w") as sharedrop:
    sharedrop.write(json.dumps(output, indent=4))
