# BTS merger allocation draft 3

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

# https://github.com/BitShares/bitshares/issues/880
substitutions = dict([
    ("1Gaw39RvbkZxcXeYzGrjWvmiEqAB6PMqsX", "1A2SAL7i5UwZ3pYuaf7rcBj1U3wffEAoo7"),
    ("13U3XLUTRHLGMwfCmhde7EmQtNdJE7X2zw", "1A2SAL7i5UwZ3pYuaf7rcBj1U3wffEAoo7"),
    ("178RVtWSyrCD8N1BnSdiSbMMokD2foQhAd", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1GJczjbQF8evXsLCHpX9sNXQ3m2QbwH2Wv", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("19btdFcvEgF6t7hyr5n4gzsGitHZjk7uF4", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1J9FPXTMJXwh1mC4CYDp8kjtgsQehiVos4", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("18Cpgt8w1aFsGBc3s82NMCU5RiRE1kiEe3", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1MnEmUHF9TYXMio86dTGXvRxeirW4VFE9w", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1DL9Rt8bWZbmNbSMZedqA2WeFRUeJS415s", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1QC9jKo53vAdo6zYUcHwpJWqGwCe5voxLv", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1JtuJEr3cGL7AyB4xcg9qMLjWV73qnHRBt", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1G3HMZzCr4QDihJEpz1arrmR4swm7krinM", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1HziNRPCtiY6HXsBouwpVzTYtZws5A25LZ", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("135xcKg36YPGi2c1yDuacgDJuxqWcporDv", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("15MeKFbkdHp7357Ez3p1jnNdGSovoBKop6", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("12tiMQ2eFAsG5SwG1xYaejigbsXiqac6hx", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1GqU61rhQ6sbwLiTc4FYSNpKZyjEA827VV", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1ED1wLdA3WW7Xr3EPRhZH6LmWP7gwJP5Qj", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1PegsunSa7ThjbEoRHxxFa5M4BgfXjbAj1", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("14seLs1EwvsXTb3dLvchFgMquJnF2yJwd2", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1NhBbVJJUPN7X451bN9FEEq4LxyWLSWcft", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("12ZoxtSZVqApmRTmY75P6jYpYDLkPJzNai", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1NZHLx6jNqY3R3953j6oALgkkZF4VoM6rH", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("14Efv8ZHhbFz1fJ3eD4tfKLyHVqAXMug7y", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("12ahkRrYhBcRGT9GbGRc7Y8uMGe9WTLibF", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("1TDLLyd684EqFHHwCuQuh5Hh1aEBZpEhv", "12Zv4sdQbm6bKpz5artZYoM8WbfFXKuTHC"),
    ("16qGZVP5rgwQCeLSnf2U94dHBa3QMNHJ3F", "12Zv4sdQbm6bKpz5artZYoM8WbfFXKuTHC"),
    ("1FnqQz36y18HK1EvaTCaGS65xUYDSTxBtx", "12Zv4sdQbm6bKpz5artZYoM8WbfFXKuTHC"),
    ("1JyiMXW7NXjsyhp4hvsm77xFn3J4tvxuZa", "16fgrvY7ACrZDLNz9GWiwPfMuk34kZWKmf"),
    ("1AEv2pKdGqJomxE9ApkW68QBg32QjjwA7b", "1BYrChoJn2UNMhTtWfRrqfcJ5ntkMYjTo8"),
    ("1KgyqkYwq1uFMwc6MTeQKsz72jJfqHSBD9", "1KLNYniYHM2UwYxSA7rjRtXqNFQEEUDhPv"),
    ("1PUZQbeu94iarZtrkKXiL4ryAEsvcsEvcE", "13P4or5Dz8734Arqg2CQLXFtxy2DSjKdXa"),
    ("1CbkZbefm25BYFeUUUQGEvt9HYWh39hvJk", "1M69AMjAkeKkk6YfuEECZzbz54EnXijzvk"),
    ("13XnJ6zKd6qgK5Uu4zJw4bdPT8M7232ZBf", "1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb"),
    ("PbXSuic9B1iEmgMiWqW93cdXFvPQsHXdUc", "PfSQYEJYKN3YTmk74BwXy6fk2StTJczaQw"),
    ("PitE7xxJvjzkVcs6BT5yRxV55YJqgFrhCU", "PfSQYEJYKN3YTmk74BwXy6fk2StTJczaQw"),

    ("Pe9F7tmq8Wxd2bCkFrW6nc4h5RASAzHAWC", "PmBR2p6uYY1SKhB9FbdHMbSGcqjEGsfK2n"),
    ("1376AFc3gfic94o9yK1dx7JMMqxzfbssrg", "1gD8zEUgPN6imT3uGUqVVnxT5agAH9r4Y")
])


# All assets have precision 10**8

# These should add up to 500m * 10**8
bts_added = 0
bts_excluded = 0

# AGS and PTS - generated using snapshot tool.
with open("ags-pts-nov-5.json") as agspts:
    snapshot = json.load(agspts)
    agspts_total = 0
    for item in snapshot["balances"]:
        balance = int(item[1])
        if item[0] == "KEYHOTEE_FOUNDERS":
            bts_excluded += balance
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            balance -= 297806854702309
            # Dan adds another vested interest
            exodus_balances.append(["1CTicY4ESkTqD5n5kFVTeNees7cq7tboXe", 297806854702309])
            print "If this line gets printed twice, it means the PaNGEL address donated to AGS!"
        agspts_total += int(item[1])
        exodus_balances.append([item[0], balance])
        bts_added += int(item[1])

# VOTE
with open("vote-aug-21-noFMV.json") as vote:
    snapshot = json.load(vote)
    vote_total = 0
    for item in snapshot["balances"]:
        if item[0] == "KEYHOTEE_FOUNDERS":
            #print "removing keyhotee ags: " + str(item[1])
            bts_excluded += int(item[1])
            continue
        vote_total += int(item[1])
        exodus_balances.append(item)
        bts_added += int(item[1])
    #print "**  vote-aug-21-noFMV.json    total:  " + str(vote_total)

# Follow my vote dev stake
with open("fmv-key.json") as fmvkey:
    items = json.load(fmvkey)
    for item in items:
        print item
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
        print "BTS per DNS normal: " + str(scale / 1000)

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
    print "bts for on-chain: " + str((bts_to_bonus - bts_for_exchanges) / 10**8)
    with open("exchanges.json") as exchanges:
        exchanges = json.load(exchanges)
        ex_total = 0
        for item in exchanges:
            ex_total += item["proportion"]
        for item in exchanges:
            print item["name"]
            bts_to_exchange = int(bts_for_exchanges * 1.0 * item["proportion"] / ex_total)
            exodus_balances.append([item["address"], bts_to_exchange])
            bts_added += bts_to_exchange
            print bts_to_exchange

    scale = 1.0 * (bts_to_bonus - bts_for_exchanges) / collapse_total
    print "BTS per DNS for normal claimed balances " + str(scale / 1000)
    print "total claimed DNS outside of exchanges " + str(collapse_total / 10**5)
    for item in collapse:
        if item[0] in devkeys:
            continue
        if item[0] == "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            continue

        balance = int(scale * int(item[1]))
        exodus_balances.append([item[0], balance])
        bts_added += balance

# bts_added and bts_excluded add up at this point. Any further changes have to adjust both balances

output = []
for item in exodus_balances:
    if item[1] != 0:
        address = item[0]
        if address in substitutions.keys():
            address = substitutions[address]
       
        # ~ 1 in 600 million chance... not a coincidence
        if address.startswith("PaNGEL") and address != "PaNGELmZgzRQCKeEKM6ifgTqNkC4ceiAWw":
            bts_added -= item[1]
            bts_excluded += item[1]
            print "Removed from pangel scammer: " + str(item[1] / 10**5)
            continue
        
        obj = {
            "raw_address": address,
            "balance": item[1]
        }
        output.append(obj)

print "bts_added: " + str(bts_added)
print "bts_excluded: " + str(bts_excluded)
print "total reviewed: " + str(bts_added + bts_excluded)

with open("bts-sharedrop.json", "w") as sharedrop:
    sharedrop.write(json.dumps(output, indent=4))


# coinbase substitutions - signatures confirmed by toast
# https://bitsharestalk.org/index.php?topic=9516.msg123741#msg123741
# Address: 178RVtWSyrCD8N1BnSdiSbMMokD2foQhAd
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: G4SbByvsoQhLVjyA3ZDwNUZW/cYO2pJK/HsaS2KgGajMUAQMZw+ZxuD82sRzq88l30fWVgn+gYoDs1uvE6gLPbY=
# 
# Address: 1GJczjbQF8evXsLCHpX9sNXQ3m2QbwH2Wv
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: GwrBVRWHmbyhFIkD5aDRxPDIS6I1HubOZEAw+9OPHAEuRS3SH6Wy6Ma9ndKDLdmc/TF279ADDLxbYr6k1ucy8ao=
# 
# Address: 19btdFcvEgF6t7hyr5n4gzsGitHZjk7uF4
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: HDdneKnMiA5dc9uKPHlPSSI53WBL0QQw43oRhKjLOePQfzPZN39gKEZObxg45Hb8MIyq6sEBO5vfY1vJaRokHjQ=
# 
# Address: 1J9FPXTMJXwh1mC4CYDp8kjtgsQehiVos4
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: HIxFt5eGHnvLe5jgqxzP6ZuKCeVJn7hb4JgVLaFOZE4vSJNJ6qrD/ScIXny2pjOqLekdoGL11st+Jd+rmXADVQQ=
# 
# Address: 18Cpgt8w1aFsGBc3s82NMCU5RiRE1kiEe3
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: HE2tfLu9bz5GQ3pNxYLmY/vIfytnblIgqmqxWKxCqRZRuMpsXra049k+vzmKU2bOcLzpZm0OKlaW+vPOA0bHe/k=
# 
# Address: 1MnEmUHF9TYXMio86dTGXvRxeirW4VFE9w
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: G8jfexkOvKP9LLtT6pzrplIVUXja/eoWsToCFMC55uqv/w5np0A9P4ijBqLrd9lKMouwHl6jlIN+qlkBXnoVCXU=
# 
# Address: 1DL9Rt8bWZbmNbSMZedqA2WeFRUeJS415s
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: G2gllIrPC6iWBB+LBHdMdPigOFC8yhShikKJwt4lv+Nwz+Ff7sQFpvaq4Z/1yui3ngnlKdi7JdkgE04WDTf5Mgs=
# 
# Address: 1QC9jKo53vAdo6zYUcHwpJWqGwCe5voxLv
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: G8j7vZOXw6EJ4/bOXAwKQppyK9YxXSgMrRwdvwnNrJu7uRajHfYN346LWiFwz0pqLkA7+vI2xJ4D7GCFXcVDlMI=
# 
# Address: 1JtuJEr3cGL7AyB4xcg9qMLjWV73qnHRBt
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: G7iO3R96/ojfmba3v9spFQEIDUYVwjd7fXpjmb7fw5uy4sOizXbyLXvOX5AUFPlLakKqDjKE5AftdC4XE8/eG+k=
# 
# Address: 1G3HMZzCr4QDihJEpz1arrmR4swm7krinM
# Message: New BTC Address: 1KfjASdNiX97R8eJM9HPbnKcFWZ8RRhzzb
# Signature: HJk5aiPFINZ9+3yKe9h7yZPuPY6yLY05T27bBReEAYDr2jCHrmDhmFzkwe8/+dtnz


#  more:  https://github.com/bitsuperlab/bitshares_play/blob/develop/libraries/blockchain/pts_ags_snapshot/substitution.txt


# educatedwarrior theft - confirmed manually by BM 

# ILuXgc0SsMz4vBni3kRBoAiWc6m2bJnUiqMpyRgNI8Zjf4n/ikFFuflV/cQv4p6PRuKxw6CwQ1mD3CC/EEki8Kw=
# "Bastard stole my PTS.  Whip his ass."
# if address == "Pe9F7tmq8Wxd2bCkFrW6nc4h5RASAzHAWC":
#    address = "PmBR2p6uYY1SKhB9FbdHMbSGcqjEGsfK2n"

# H0eoaUht5pgKeO0W6U+graUn2kkyTxybb5ttkdZ8BxOBamghu0vB/ZbBw4329LbzKIZIoH4QtTTLPAFBqmX6IR4=
# "Dan is the man with the plan!"  - same person
# if address == "1376AFc3gfic94o9yK1dx7JMMqxzfbssrg":
#    address = "1gD8zEUgPN6imT3uGUqVVnxT5agAH9r4Y"


