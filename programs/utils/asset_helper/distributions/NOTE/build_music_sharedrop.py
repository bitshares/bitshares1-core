import bitsharesrpc
import sys
import json
import subprocess

if len(sys.argv) < 2:
    print "Usage: python build_music_snapshot.py <agspts file> <presale>\n\n agsptsfile comes from http://ptsags.quisquis.de/"

snapshot = {}

def add_balance(addr, bal):
    if addr in snapshot:
        snapshot[addr] += bal
    else:
        snapshot[addr] = bal

with open(sys.argv[1]) as agsptsfile:
    agspts = json.load(agsptsfile)
    total = 0
    for pair in agspts["balances"]:
        total += pair[1]
    print total
    ratio = 1.0 * 10500000000000 / total
    print "WARNING temporary ratio"
    print ratio

    for pair in agspts["balances"]:
        add_balance(pair[0], int(ratio * pair[1]))

with open("notes-allocation.json", "w") as outfile:
    outfile.write(json.dumps(snapshot, indent=4))

