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
            "spread_percent": 0.05,
            "external_price_tolerance": 0.01,
            "min_order_size": 100,
            "min_balance": 1000,
            "block_shorts": True,
            "asset_pair": ["USD", "BTSX"]
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
        return new_config("default_config.json")
    with open(path) as f:
        return json.load(f)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Usage:  python config.py new_config_path"
        sys.exit(1)

    path = sys.argv[1]
    new_config(path)
