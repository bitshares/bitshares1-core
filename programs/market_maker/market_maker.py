from mylog import logger
log = logger.log


# This bot provides liquidity and maintains the peg

MEDIAN_EDGE_MULTIPLE = 1.001

class MarketMaker():
    def __init__(self, client, feeds, bot_config):
        self.client = client
        if not "usd_per_btsx" in feeds:
            raise Exception("Missing required feed 'usd_per_btsx' for market maker bot")
        self.feeds = feeds
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
        min_order_size = self.config["min_order_size"]
        quote = self.quote_symbol
        base = self.base_symbol


        true_usd_per_btsx = self.feeds["usd_per_btsx"].fetch()
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

        if available_usd_buy_quantity > min_order_size:
            log("Submitting a bid...")
            self.client.submit_bid(self.name, available_usd_buy_quantity, base, new_usd_per_btsx, self.quote_symbol)
        else:
            log("Skipping bid - USD balance too low")

        if available_btsx_balance > min_order_size:
            log("submitting an ask...")
            self.client.submit_ask(self.name, available_btsx_balance, base, new_usd_per_btsx * (1+spread), self.quote_symbol)
        else:
            log("Skipping ask - BTSX balance too low")

