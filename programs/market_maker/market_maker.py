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
        log("Executing bot:  %s" % self.name)
        tolerance = self.config["external_price_tolerance"]
        spread = self.config["spread_percent"]
        min_balance = self.config["min_balance"]
        quote = self.quote_symbol
        base = self.base_symbol


        true_usd_per_btsx = get_true_price()
        median = self.client.get_median(self.quote_symbol)
        new_usd_per_btsx = true_usd_per_btsx

        if median > new_usd_per_btsx:
            new_usd_per_btsx = median * MEDIAN_EDGE_MULTIPLE

        self.client.cancel_asks_out_of_range(self.name, quote, base, new_usd_per_btsx * (1+spread), tolerance)

        self.client.cancel_bids_less_than(self.name, quote, base, median)
        self.client.cancel_bids_out_of_range(self.name, quote, base, new_usd_per_btsx, tolerance)

        log("waiting for a block...")
        self.client.wait_for_block()

        usd_balance = self.client.get_balance(self.name, quote)
        btsx_balance = self.client.get_balance(self.name, base)
        available_btsx_balance = btsx_balance - min_balance
        available_usd_buy_quantity = (usd_balance / new_usd_per_btsx) - min_balance;

        if available_usd_buy_quantity > MIN_BTSX_ORDER_SIZE:
            log("Submitting a bid...")
            self.client.submit_bid(self.name, available_usd_buy_quantity, base, new_usd_per_btsx, self.quote_symbol)
        else:
            log("Skipping bid - USD balance too low")

        if available_btsx_balance > MIN_BTSX_ORDER_SIZE:
            log("submitting an ask...")
            self.client.submit_ask(self.name, available_btsx_balance, base, new_usd_per_btsx * (1+spread), self.quote_symbol)
        else:
            log("Skipping ask - BTSX balance too low")

