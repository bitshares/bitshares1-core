class Feed():

    @staticmethod
    def from_func(f):
        class NewFeed(Feed):
            def fetch(self):
                return f()
        return NewFeed({})

    def __init__(self, feeds):
        self.feeds = feeds # dependencies

    def fetch(self):
        return None
