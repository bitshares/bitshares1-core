
bids = set()
asks = set()

bids.add((0, 1))
bids.add((1, 1))
bids.add((0, 2))
bids.add((1, 2))


for item in sorted(bids):
    print item


class MarketSim():

    # this just uncrosses the books by popping off one order from each side
    # in alternating order. This doesn't mimic a real market very accurately
    def match():
        pass
