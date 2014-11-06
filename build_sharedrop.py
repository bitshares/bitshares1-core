import json

exodus_balances = []
bts_added = 0
bts_removed = 0

# AGS and PTS
with open("ags-pts-nov-5.json") as agspts:
    snapshot = json.load(agspts)
    agspts_total = 0
    for item in snapshot["balances"]:
        if item[0] == "KEYHOTEE_FOUNDERS":
            #print "removing keyhotee ags: " + str(item[1])
            bts_removed += int(item[1])
            continue
        agspts_total += int(item[1])
        exodus_balances.append(item)
        bts_added += int(item[1])
    #print "**  ags-pts-nov-5.json        total: " + str(agspts_total)

# VOTE
with open("vote-aug-21-noFMV.json") as vote:
    snapshot = json.load(vote)
    vote_total = 0
    for item in snapshot["balances"]:
        if item[0] == "KEYHOTEE_FOUNDERS":
            #print "removing keyhotee ags: " + str(item[1])
            bts_removed += int(item[1])
            continue
        vote_total += int(item[1])
        exodus_balances.append(item)
        bts_added += int(item[1])
    #print "**  vote-aug-21-noFMV.json    total:  " + str(vote_total)


pangel_dns = 0
dns_no_dev = 0
dns_scaled_total = 0
bts_to_dns_normal = 0
bts_to_bonus = 0

# DNS normal - remove pangel and dev funds
with open("dns-dev-fund.json") as devfund:
    devkeys = json.load(devfund)
    with open("dns-nov-5.json") as dns:
        snapshot = json.load(dns)
        for item in snapshot:
            if (item[0] in devkeys and item[1] != 0):
                #print "skipping dev balance: " + str(item[1])
                continue
            if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
                pangel_dns = item[1]
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
    bts_added += bts_to_bonus

# Follow my vote dev stake
with open("fmv-key.json") as fmvkey:
    item = json.load(fmvkey)[0]
    exodus_balances.append(item)
    bts_added += item[1]
    print "FMV allocation: " + str(item[1])

print "bts to bonus: " + str(bts_to_bonus / (10**8))

# DNS extra - add pangel funds and exchange subsidy
with open("dns-collapse.json") as collapse:
    collapse_total = 0
    for item in json.load(collapse):
        if (item[0] in devkeys and item[1] != 0):
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            continue
        collapse_total += int(item[1])
  
    bts_for_exchanges = 100000
    bts_added += bts_for_exchanges
    scale = (bts_to_bonus - bts_for_exchanges) / collapse_total
    for item in collapse:
        if item[0] in devkeys:
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            continue

        balance = int(scale * int(item[1]))
        exodus_balances.append([item[0], balance])
        bts_added += balance

print "bts_added: " + str(bts_added)
print "bts_removed: " + str(bts_removed)
print "total reviewed: " + str(bts_added + bts_removed)


with open("libraries/blockchain/bts-sharedrop.json", "w") as sharedrop:
    sharedrop.write(json.dumps(exodus_balances, indent=4))
