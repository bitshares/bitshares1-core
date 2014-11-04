import json

exodus_balances = []

# AGS and PTS
with open("ags-pts-nov-1.json") as agspts:
    exodus_balances.extend(json.load(agspts)["balances"])

# VOTE
with open("vote-aug-21-noFMV.json") as vote:
    exodus_balances.extend(json.load(vote)["balances"])
# FMV

# DNS normal - remove pangel and dev funds
# DNS extra - remove dev funds

with open("libraries/blockchain/bts-sharedrop.json", "w") as sharedrop:
    sharedrop.write(json.dumps(exodus_balances, indent=4))
