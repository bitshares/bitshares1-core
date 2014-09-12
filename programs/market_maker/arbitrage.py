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



