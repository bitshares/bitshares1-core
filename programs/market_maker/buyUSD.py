import sys
from btsx import BTSX
from mylog import logger
import urllib2
import json
import time

if len(sys.argv) < 5:
    print "Usage:  feed.py rpcuser rpcpass port live_network"
    sys.exit(0)

user = sys.argv[1]
password = sys.argv[2]
port = int(sys.argv[3])
live = bool(sys.argv[4])

SYMBOL = ""
if live:
    SYMBOL = "BTSX"
else:
    SYMBOL = "XTS"

client = BTSX(user, password, port, "bitusd-buyer")


while True:
    remaining = client.get_balance(SYMBOL)
    bid = client.get_highest_bid("USD", SYMBOL)
    price = (float(ask["market_index"]["order_price"]["ratio"]) * 10)  - 0.0001
    print client.submit_ask(remaining / 100, SYMBOL, price, "USD")
    time.sleep(10)
