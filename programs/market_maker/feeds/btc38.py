import requests
import json
import time

class BTC38Feeds():
    def __init__(self, feeds={}):
        self.headers = {
            'content-type': 'application/json',
            'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:22.0) Gecko/20100101 Firefox/22.0'
        }
        self.price = {
            "BTC": [],
            "USD": [],
            "CNY": [],
            "GLD": []
        }

        self.rate_usd_cny = 0
        self.rate_xau_cny = 0
        self.get_rate_from_yahoo()

    def last_usd_per_btsx(self):
        url="http://api.btc38.com/v1/ticker.php"
        try:
            params = { 'c': 'btsx', 'mk_type': 'btc' }
            responce = requests.get(url=url, params=params, headers=self.headers)
            result = responce.json()
            self.price["BTC"].append(float(result["ticker"]["last"]))
            params = { 'c': 'btsx', 'mk_type': 'cny' }
            responce = requests.get(url=url, params=params, headers=self.headers)
            result = responce.json()
            price_cny = float(result["ticker"]["last"])
            self.price["CNY"].append(price_cny)
            self.price["USD"].append(price_cny/self.rate_usd_cny)
            self.price["GLD"].append(price_cny/self.rate_xau_cny)
            return self.price["USD"][-1]
        except Exception, e:
            print "error in btc38 feed"
            print e
            return None

    def get_rate_from_yahoo(self):
        try:
            url="http://download.finance.yahoo.com/d/quotes.csv"
            params = {'s':'USDCNY=X,XAUCNY=X','f':'l1','e':'.csv'}
            responce = requests.get(url=url, headers=self.headers,params=params)
            pos = posnext = 0
            posnext = responce.text.find("\n", pos)
            self.rate_usd_cny = float(responce.text[pos:posnext])
            pos = posnext + 1
            posnext = responce.text.find("\n", pos)
            self.rate_xau_cny = float(responce.text[pos:posnext])
        except Exception, e:
            print e
            print "Warning: unknown error, try again after 1 seconds"
            time.sleep(1)
            self.get_rate_from_yahoo()
