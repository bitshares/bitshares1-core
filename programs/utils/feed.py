import sys
import requests
import json
import urllib2
import time

if len(sys.argv) < 5:
    print "Usage:  feed.py rpcuser rpcpass port \"['delegate','list']\""
    sys.exit(0)

user = sys.argv[1]
password = sys.argv[2]
port = int(sys.argv[3])
delegates = []
if len(sys.argv[4]) < 3:
    for i in range(60):
       delegates.append("init" + str(i))
else:
    delegates = eval(sys.argv[4])


print delegates


while True:
    url = 'https://www.bitstamp.net/api/ticker/'
    hdr = {'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.11 (KHTML, like Gecko) Chrome/23.0.1271.64 Safari/537.11',
           'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
           'Accept-Charset': 'ISO-8859-1,utf-8;q=0.7,*;q=0.3',
           'Accept-Encoding': 'none',
           'Accept-Language': 'en-US,en;q=0.8',
           'Connection': 'keep-alive'}

    req = urllib2.Request(url, headers=hdr)

    bitstamp_data = json.load(urllib2.urlopen(req))
    usd_per_btc = float(bitstamp_data["last"])

    url = 'http://data.bter.com/api/1/ticker/bts_btc'
    req = urllib2.Request(url, headers=hdr)
    btsx_data = json.load(urllib2.urlopen(req))
    btc_per_btsx = float(btsx_data["last"])

    usd_per_btsx = usd_per_btc * btc_per_btsx
    print usd_per_btsx


    url = "http://" + user + ":" + password + "@localhost:" + str(port) + "/rpc"
    print url

    count = 0
    for name in delegates:
        payload = {
            "method": "wallet_transfer",
            "params": [5, "XTS", "init0", "init" + str(count)],
            "jsonrpc": "2.0",
            "id": 0,
        }

        print payload

        headers = {
          'content-type': 'application/json',
          'Authorization': "Basic YTph"
        }
        response = requests.post(url, data=json.dumps(payload), headers=headers)
        print response
        print response.json()

        count += 1


    for name in delegates:
        payload = {
            "method": "wallet_publish_price_feed",
            "params": [name, usd_per_btsx, "USD"],
            "jsonrpc": "2.0",
            "id": 0,
        }

        print payload

        headers = {
          'content-type': 'application/json',
          'Authorization': "Basic YTph"
        }
        response = requests.post(url, data=json.dumps(payload), headers=headers)
        print response
        print response.json()

        payload = {
            "method": "wallet_publish_price_feed",
            "params": [name, btc_per_btsx, "BTC"],
            "jsonrpc": "2.0",
            "id": 0,
        }

        print payload

        headers = {
          'content-type': 'application/json',
          'Authorization': "Basic YTph"
        }
        response = requests.post(url, data=json.dumps(payload), headers=headers)
        print response
        print response.json()




    time.sleep(30)
