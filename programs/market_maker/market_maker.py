from mylog import logger
from prices import get_true_price
log = logger.log


# This bot provides liquidity and maintains the peg

SPREAD_PERCENT = 0.05
TOLERANCE = 0.01 # should be less than SPREAD_PERCENT / 2

MIN_USD_BALANCE = 10
MIN_BTSX_BALANCE = 100
MIN_USD_ORDER_SIZE = 2
MIN_BTSX_ORDER_SIZE = 100

MEDIAN_EDGE_MULTIPLE = 1.001

class MarketMaker():
    def __init__(self, client, bot_config):
        self.client = client
        if not "bot_type" in bot_config or bot_config["bot_type"] != "market_maker":
            raise Exception("Bad bot configuration object")
        self.config = bot_config
        self.name = self.config["account_name"]
        self.quote_symbol = bot_config["asset_pair"][0]
        self.base_symbol = bot_config["asset_pair"][1]


    def execute(self):
        true_usd_per_btsx = get_true_price()

        median = self.client.get_median(self.quote_symbol)
        new_usd_per_btsx = true_usd_per_btsx
        if median > new_usd_per_btsx:
            new_usd_per_btsx = median * MEDIAN_EDGE_MULTIPLE

        self.client.cancel_asks_out_of_range(self.name, self.qote_symbol, self.base_symbol, new_usd_per_btsx * (1+self.config["spread_percent"]), self.config["tolerance"])
        self.client.cancel_bids_less_than(self.name, self.quote_symbol, self.base_symbol, median)
        self.client.cancel_bids_out_of_range(self.name, self.quote_symbol, self.base_symbol, new_usd_per_btsx, self.config["tolerance"])

        self.client.wait_for_block()

        usd_balance = client.get_balance(self.quote_symbol)
        btsx_balance = client.get_balance(self.base_symbol)
        available_btsx_balance = btsx_balance - self.config["min_balance"]
        available_usd_buy_quantity = (usd_balance / new_usd_per_btsx) - self.config["min_balance"];

        if available_usd_buy_quantity > MIN_BTSX_ORDER_SIZE:
            self.client.submit_bid(self.name, available_usd_buy_quantity, self.base_symbol, new_usd_per_btsx, self.quote_symbol)
        if available_btsx_balance > MIN_BTSX_ORDER_SIZE:
            self.client.submit_ask(self.name, available_btsx_balance, SYMBOL, new_usd_per_btsx * (1+SPREAD_PERCENT), self.quote_symbol)

