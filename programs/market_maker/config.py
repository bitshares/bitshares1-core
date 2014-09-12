
default_config = {
    "client": {  # BTSX client config
        "rpc_user": "",
        "rpc_password": "",
        "rpc_port": ""
    },
    "bots": [
        {  # Maintains a spread around the real price
            "account_name": "bitusd_bot",
            "bot_type": "market_maker",
            "spread_percent": 0.05, # Buy BitUSD undervalued by this %
            "external_price_tolerance": 0.01, # don't adjust orders until price moves this %
            "min_order_size": 1000, #  don't make orders smaller than this much BTSX worth
            "min_balance": 100, # keep at least this much BTSX for bot expense buffer
            #  "block_shorts": True,    prevent new shorts from entering - default behavior for now
            "asset_pair": ["USD", "BTSX"]
        }
    ],
    "feeds": [
        {
            "name": "bter_usd_btsx_average",
            "type": "usd_per_btsx",
        }
    ]
}



import os
import json
import sys


def new_config(path):
    with open(path, 'w') as f:
        f.write(json.dumps(default_config, indent=4))
    return default_config

def read_config(path):
    if not os.path.isfile(path):
        return new_config(path)
    with open(path) as f:
        return json.load(f)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Usage:  python config.py new_config_path"
        sys.exit(1)

    path = sys.argv[1]
    new_config(path)
