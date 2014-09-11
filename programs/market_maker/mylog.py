import time

class L():
    current_log = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = super(Singleton, cls).__new__(
                                cls, *args, **kwargs)
        return cls._instance
    def __init__(self):
        if self.current_log:
            self.logfile = open("logs/" + current_log, "w")
        else:
            self.current_log = str(time.time())
            self.logfile = open("logs/" + self.current_log, "w")
    def log(self, msg):
        self.logfile.write(str(msg) + "\n")
        print msg

logger = L()
