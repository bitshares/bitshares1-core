#!/usr/bin/env python3

import json
import os
import subprocess

def mkdir_p(path):
    if path == "":
        return
    try:
        os.makedirs(path)
    except FileExistsError:
        pass
    return

def create_keys(seed, count):
    keyout = subprocess.check_output(
    [
     "programs/utils/bts_create_key",
     "--count="+str(count),
     "--seed="+seed,
    ],
    )
    return json.loads(keyout.decode())

genesis_filename = "tmp/genesis.json"

init_keys = create_keys("init", 101)
angel_keys = create_keys("angel", 10)

names = ["alice", "bob", "charlie", "dan", "emily", "fox", "galt", "henry", "isabelle", "janet", "kate", "lisa", "matt", "nate", "otto", "pam", "quixote", "ron", "stan", "tony", "ulysses", "vik", "walt", "xavier", "yoshi", "zack"]

name_keys = []

for name in names:
    name_keys.extend(create_keys(name, 10))

init_balance  = 300000000000
angel_balance = 2000000000000
name_balance  = 10000000000

balances = [[k["pts_address"],  init_balance] for k in  init_keys
         ]+[[k["pts_address"], angel_balance] for k in angel_keys
         ]+[[k["pts_address"],  name_balance] for k in  name_keys]

BTS_BLOCKCHAIN_MAX_SHARES = (1000*1000*1000*1000*1000)
BTS_BLOCKCHAIN_INITIAL_SHARES = (BTS_BLOCKCHAIN_MAX_SHARES // 5)

total_balance = sum(b[1] for b in balances)

ballast_keys = create_keys("ballast", 1)
ballast_balance = BTS_BLOCKCHAIN_INITIAL_SHARES - total_balance

balances += [[k["pts_address"], ballast_balance] for k in ballast_keys]

genesis_json = {
  "timestamp" : "2014-02-02T18:00:00",

  "market_assets" : [
    {
        "symbol" : "USD",
        "name" : "United States Dollar",
        "descrption" : "Federally Reserved, Inflation Guaranteed",
        "precision" : 10000
    },
    {
        "symbol" : "BTC",
        "name" : "Bitcoin",
        "description" : "This is an internet cryptocurrency.  All craftdwarfship is of the highest quality.  On the item is an image of Urist Nakamoto the dwarf in silicon.",
        "precision" : 100000000
    },
    {
        "symbol" : "GAS",
        "name" : "Gasoline",
        "description" : "Secondary fuel source used by some electric vehicles",
        "precision" : 10000
    }
  ],
  
  "names" :
  [
    {
      "name" : "init"+str(i),
      "owner" : init_keys[i]["public_key"],
      "delegate_pay_rate" : 1,
    }
    for i in range(101)
  ],

  "balances" : balances,
}

bts_sharedrop = []

mkdir_p(os.path.dirname(genesis_filename))
with open(genesis_filename, "w") as f:
    json.dump(genesis_json, f, indent=4)
