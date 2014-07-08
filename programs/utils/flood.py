#!/usr/bin/python

import requests
import json
from pprint import pprint
import time


def main() :
    url = "http://localhost:8899/rpc"
    headers = {'content-type': 'application/json'}
    payload2 = {
        "method": "get_info",
        "params": [],
        "jsonrpc": "2.0",
        "id": 10
    }

    i = 1
    while True :
        print "sending 1 transactions"
        for j in range(11):
            payload = {
                "method": "wallet_transfer",
                "params": [0.1, "XTS", "founders", "founders", "id:%d.%d"%(i,j)],
                "jsonrpc": "2.0",
                "id": i
            }
            auth = ('user', 'password')

            response = requests.post(url, data=json.dumps(payload), headers=headers, auth=auth)
            pprint(vars(response))
        time.sleep(1)
        i = i + 1


if __name__ == "__main__":
    main()
