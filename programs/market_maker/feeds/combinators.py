from feed import Feed

class Average(Feed):
    def fetch(self):
        total = 0.0
        valid = 0
        for key, feed in self.feeds.iteritems():
            value = feed.fetch()
            if value:
                total += value
                valid += 1
        if valid == 0:
            return None
        return total / valid

class Median(Feed):
    pass
