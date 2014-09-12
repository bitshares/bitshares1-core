import urllib2
import json
import requests
from mylog import logger
import time

log = logger.log

class BTSX():
    USD_PRECISION = 10000.0
    BTSX_PRECISION = 100000.0
    
    def __init__(self, user, password, port):
        self.url = "http://" + user + ":" + password + "@localhost:" + str(port) + "/rpc"
        log("Initializing with URL:  " + self.url)

    def request(self, method, *args):
        payload = {
            "method": method,
            "params": list(*args),
            "jsonrpc": "2.0",
            "id": 0,
        }
        headers = {
          'content-type': 'application/json',
          'Authorization': "Basic YTph"
        }
        response = requests.post(self.url, data=json.dumps(payload), headers=headers)
        return response

    def get_median(self, asset):
        response = self.request("blockchain_get_feeds_for_asset", [asset])
        feeds = response.json()["result"]
        return feeds[len(feeds)-1]["median_price"]

    def submit_bid(self, account_name, amount, quote, price, base):
        response = self.request("bid", [account_name, amount, quote, price, base])
        if response.status_code != 200:
            log("%s submitted a bid" % account_name)
            log(response.json())
            return False
        else:
            return response.json()
    def submit_ask(self, account_name, amount, quote, price, base):
        response = self.request("ask", [account_name, amount, quote, price, base])
        if response.status_code != 200:
            log("%s submitted an ask" % account_name())
            log(response.json())
            return False
        else:
            return response.json()
    def get_lowest_ask(self, asset1, asset2):
        response = self.request("blockchain_market_order_book", [asset1, asset2])
        return response.json()["result"][0][0]
        
    def get_balance(self, account_name, asset):
        asset_id = 22
        if asset == "BTSX":
            asset_id = 0

        response = self.request("wallet_account_balance", [account_name, asset])
        if not response.json():
            log("Error in get_balance: %s", response["_content"]["message"])
            return None

        asset_array = response.json()["result"][0][1]
        amount = 0
        for item in asset_array:
            if item[0] == asset_id:
                amount = item[1]
        if asset == "USD":
            return amount / self.USD_PRECISION
        if asset == "BTSX":
            return amount / self.BTSX_PRECISION
        log("UNKNOWN ASSET TYPE, CANT CONVERT PRECISION: %s" % asset)
        exit(1)

    def cancel_bids_less_than(self, account, base, quote, price):
        response = self.request("wallet_market_order_list", [base, quote, -1, account])
        order_ids = []
        for pair in response.json()["result"]:
            order_id = pair[0]
            item = pair[1]
            if item["type"] == "bid_order":
                if float(item["market_index"]["order_price"]["ratio"])* (self.BTSX_PRECISION / self.USD_PRECISION) < price:
                    order_ids.append(order_id)
                    log("%s canceled an order: %s" % (account_name, str(item)))
        cancel_args = [[item] for item in order_ids]
        response = self.request("batch", ["wallet_market_cancel_order", cancel_args])
        return cancel_args

    def cancel_bids_out_of_range(self, account, base, quote, price, tolerance):
        response = self.request("wallet_market_order_list", [base, quote, -1, account])
        order_ids = []
        for pair in response.json()["result"]:
            order_id = pair[0]
            item = pair[1]
            if item["type"] == "bid_order":
                if abs(price - float(item["market_index"]["order_price"]["ratio"]) * (self.BTSX_PRECISION / self.USD_PRECISION)) > price*tolerance:
                    order_ids.append(order_id)
                    log("%s canceled an order: %s" % (account_name, str(item)))
        cancel_args = [[item] for item in order_ids]
        response = self.request("batch", ["wallet_market_cancel_order", cancel_args])
        return cancel_args

    def cancel_asks_out_of_range(self, account, base, quote, price, tolerance):
        response = self.request("wallet_market_order_list", [base, quote, -1, account])
        order_ids = []
        for pair in response.json()["result"]:
            order_id = pair[0]
            item = pair[1]
            if item["type"] == "ask_order":
                if abs(price - float(item["market_index"]["order_price"]["ratio"]) * (self.BTSX_PRECISION / self.USD_PRECISION)) > price*tolerance:
                    order_ids.append(order_id)
        cancel_args = [[item] for item in order_ids]
        response = self.request("batch", ["wallet_market_cancel_order", cancel_args])
        return cancel_args

    def cancel_all_orders(self, account, base, quote):
        response = self.request("wallet_market_order_list", [base, quote, -1, account])
        order_ids = []
        for item in response.json()["result"]:
            order_ids.append(item["market_index"]["owner"])
        cancel_args = [[item] for item in order_ids]
        response = self.request("batch", ["wallet_market_cancel_order", cancel_args])
        return cancel_args


    def wait_for_block(self):
        response = self.request("get_info", [])
        blocknum = response.json()["result"]["blockchain_head_block_num"]
        while True:
            time.sleep(0.1)            
            response = self.request("get_info", [])
            blocknum2 = response.json()["result"]["blockchain_head_block_num"]
            if blocknum2 != blocknum:
                return
