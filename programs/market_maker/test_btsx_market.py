import sys
from btsx import BTSX
from mylog import logger

log = logger.log

if len(sys.argv) < 4:
    print "Usage:  feed.py rpcuser rpcpass port "
    sys.exit(0)

user = sys.argv[1]
password = sys.argv[2]
port = int(sys.argv[3])

client = BTSX(user, password, port)

client.cancel_all_orders("USD", "XTS")

#amount = client.get_balance("XTS")
#print amount

test = client.submit_bid(100, "XTS", 0.01, "USD")
print test

test = client.cancel_all_orders("USD", "XTS")
print test

#test = client.submit_ask(100, "XTS", 0.01, "USD")
#print test

#amount = client.get_balance("USD")
#print amount
