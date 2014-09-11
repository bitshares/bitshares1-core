import sys
from btsx import BTSX
from mylog import logger
import urllib2
import json
import time



## Two exchange one asset simple arbitrage
# check for crossed prices, increment timer for exchange pair
# execute appropriate trades if prices crossed long enough
# bookkeeping: rebalance accounts, record transactios

class MarketSource():
    
    def get_highest_bid(base, quote):
        pass

    def get_lowest_ask(base, quote):
        pass

    def submit_bid(base, quote, amount, price):
        pass

    def submit_ask(base, quote, amount, price):
        pass

bter = MarketSource()
btc38 = MarketSource()

#while True:
#    bter_highest_bid = bter.get_highest_bid(BTSX, BTC)
#    btc38_lowest_ask = btc38.get_lowest_ask(BTSX, BTC)
#    if bter_highest_bid.price > btc38_loest_ask.price:
#        amount = min(bter_highest_bid.amount, btc38_lowest_ask.amount)
#        bter.submit_bid(BTSX, BTC, amount, bter_highest_bid.price)
#        btc38.submit_ask(BTSX, BTC, amount, btc38_lowest_ask.price)



## BitUSD market maker

def get_true_price():
    url = 'https://www.bitstamp.net/api/ticker/'
    hdr = {'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.11 (KHTML, like Gecko) Chrome/23.0.1271.64 Safari/537.11',
           'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
           'Accept-Charset': 'ISO-8859-1,utf-8;q=0.7,*;q=0.3',
           'Accept-Encoding': 'none',
           'Accept-Language': 'en-US,en;q=0.8',
           'Connection': 'keep-alive'}

    req = urllib2.Request(url, headers=hdr)

    bitstamp_data = json.load(urllib2.urlopen(req))
    usd_per_btc_ask = float(bitstamp_data["ask"])
    usd_per_btc_bid = float(bitstamp_data["bid"])
    usd_per_btc = (usd_per_btc_ask + usd_per_btc_bid) / 2

    url = 'http://data.bter.com/api/1/ticker/btsx_btc'
    req = urllib2.Request(url, headers=hdr)
    btsx_data = json.load(urllib2.urlopen(req))
    btc_per_btsx_buy = float(btsx_data["buy"])
    btc_per_btsx_sell = float(btsx_data["sell"])
    btc_per_btsx = (btc_per_btsx_buy + btc_per_btsx_sell) / 2

    usd_per_btsx = usd_per_btc * btc_per_btsx

    return usd_per_btsx

def log(msg):
    print msg


SPREAD_PERCENT = 0.05
TOLERANCE = 0.02 # should be less than SPREAD_PERCENT / 2

MIN_USD_BALANCE = 100
MIN_BTSX_BALANCE = 1000
MIN_USD_ORDER_SIZE = 100
MIN_BTSX_ORDER_SIZE = 3000

MEDIAN_EDGE_MULTIPLE = 1.001

log = logger.log

if len(sys.argv) < 5:
    print "Usage:  feed.py rpcuser rpcpass port live_network"
#    sys.exit(0)

user = sys.argv[1]
password = sys.argv[2]
port = int(sys.argv[3])
live = bool(sys.argv[4])

if live:
    SYMBOL = "BTSX"
else:
    SYMBOL = "XTS"

client = BTSX(user, password, port, "arbiteur")


init_price = get_true_price()
log("Init price:  %f" % init_price)
log("Init price:  %f" % (1.0 / init_price))
init_usd_balance = client.get_balance("USD")
print init_usd_balance
init_btsx_balance = client.get_balance(SYMBOL)
print init_btsx_balance

client.cancel_all_orders("USD", SYMBOL)
print init_usd_balance / init_price
print client.submit_bid(0.3*(init_usd_balance / init_price), SYMBOL, init_price * (1-SPREAD_PERCENT), "USD")
print client.submit_ask(0.3*(init_btsx_balance), SYMBOL, init_price * (1+SPREAD_PERCENT), "USD")
sec_since_update = 0
last_price = init_price
while True:
    true_usd_per_btsx = get_true_price()

    log("Price moved  -  old:  %f   new:  %f" % (last_price, true_usd_per_btsx))

    median = client.get_median("USD")
    new_usd_per_btsx = true_usd_per_btsx
    if median > new_usd_per_btsx:
        new_usd_per_btsx = median * MEDIAN_EDGE_MULTIPLE

    client.cancel_asks_out_of_range("USD", SYMBOL, new_usd_per_btsx * (1+SPREAD_PERCENT), TOLERANCE)
    client.cancel_bids_out_of_range("USD", SYMBOL, new_usd_per_btsx, TOLERANCE)

    client.wait_for_block()

    usd_balance = client.get_balance("USD")
    btsx_balance = client.get_balance(SYMBOL)
    available_usd_balance = usd_balance - MIN_USD_BALANCE
    available_btsx_balance = btsx_balance - MIN_BTSX_BALANCE
    if available_usd_balance > MIN_USD_ORDER_SIZE:
        client.submit_bid((available_usd_balance, SYMBOL, new_usd_per_btsx, "USD"))
    if available_btsx_balance > MIN_BTSX_ORDER_SIZE:
        client.submit_ask(available_btsx_balance*true_usd_per_btsx, SYMBOL, new_usd_per_btsx * (1+SPREAD_PERCENT), "USD")
    last_price = new_price

