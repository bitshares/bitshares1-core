import urllib2
import requests
import json

def get_bter_price(base, quote):
    pass

def get_btc38_price(base, quote):
    url="http://api.btc38.com/v1/ticker.php"
    try:
        params = { 'c': 'btsx', 'mk_type': 'btc' }
        responce = requests.get(url=url, params=params, headers=headers)
        result = responce.json()
        price["BTC"].append(float(result["ticker"]["last"]))
        params = { 'c': 'btsx', 'mk_type': 'cny' }
        responce = requests.get(url=url, params=params, headers=headers)
        result = responce.json()
        price_cny = float(result["ticker"]["last"])
        price["CNY"].append(price_cny)
        price["USD"].append(price_cny/rate_usd_cny)
        price["GLD"].append(price_cny/rate_xau_cny)
    except:
        print "Warning: unknown error"
        returns
        pass

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


