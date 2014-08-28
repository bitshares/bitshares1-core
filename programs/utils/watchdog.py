#!/usr/bin/env python3.4

# This is a watchdog script which connects to a delegate periodically to verify that it is up and running,
# and to do some basic maintenance tasks if the delegate is not ready to sign blocks and report if it is
# unhealthy

# Written by Nathan Hourt. If this script is useful to you, consider voting for delegate.nathanhourt.com :)

import os
import socket
import json
import getpass
import time
import datetime

RPC_USERNAME = "test"
RPC_PASSWORD = "testify"
WALLET_NAME = "default"
HOST = "localhost"
PORT = 1212

MAX_ALLOWABLE_HEAD_BLOCK_AGE = datetime.timedelta(minutes=2)

if "DELEGATE_RPC_USERNAME" in os.environ:
  RPC_USERNAME = os.environ["DELEGATE_RPC_USERNAME"]
if "DELEGATE_RPC_PASSWORD" in os.environ:
  RPC_PASSWORD = os.environ["DELEGATE_RPC_PASSWORD"]
if "DELEGATE_WALLET_NAME" in os.environ:
  WALLET_NAME = os.environ["DELEGATE_WALLET_NAME"]
if "DELEGATE_PORT_1212_TCP_ADDR" in os.environ:
  HOST = os.environ["localhost"]
if "DELEGATE_PORT_1212_TCP_PORT" in os.environ:
  PORT = int(os.environ["DELEGATE_PORT_1212_TCP_PORT"])

request = {"method":"",
           "params":[],
           "id":0}

passphrase = getpass.getpass("Please enter your delegate's wallet passphrase: ")

def parse_date(date):
  return datetime.datetime.strptime(date, "%Y%m%dT%H%M%S")

def connect():
  sock = socket.socket()
  sock.connect((HOST,PORT))
  return sock
sock = connect()

def call(method, params=[]):
  request["method"] = method
  request["params"] = params
  request["id"] += 1

  sock.send(json.dumps(request).encode('UTF-8'))
  response = json.loads(sock.recv(100000).decode('UTF-8'))

  if response["id"] != request["id"]:
    print("Sent request with id %d, got response with id %d... Retrying." % (request["id"], response["id"]))
    sock.send(json.dumps(request).encode('UTF-8'))
    response = json.loads(sock.recv(100000).decode('UTF-8'))

    if response["id"] != request["id"]:
      print("Nope, still didn't work. Here's the request that failed:")
      print(request)
      print("And the response:")
      print(response)
    
  return response

result = call("login", [RPC_USERNAME, RPC_PASSWORD])
if "error" in result or not result["result"]:
  print("Failed to login to RPC server:")
  print(result["error"])
  exit(1)

while True:
  try:
    print("\n\nRunning Watchdog")
 
    response = call("get_info")
    if "error" in response:
      print("FATAL: Failed to get info:")
      print(result["error"])
      exit(1)
    response = response["result"]
    
    if "wallet_open" not in response or not response["wallet_open"]:
      print("Opening wallet.")
      result = call("open", [WALLET_NAME])
      if "error" in result:
        print("Failed to open wallet:")
        print(result["error"])
 
    if "wallet_unlocked" not in response or not response["wallet_unlocked"]:
      print("Unlocking wallet.")
      result = call("unlock", [9999999999, passphrase])
      if "error" in result:
        print("Failed to unlock wallet:")
        print(result["error"])
        passphrase = getpass.getpass("Please enter your delegate's wallet passphrase: ")
        continue
    
    if "wallet_block_production_enabled" not in response or not response["wallet_block_production_enabled"]:
      print("Enabling block production for all delegates.")
      result = call("wallet_delegate_set_block_production", ["ALL", True])
      if "error" in result:
        print("Failed to enable block production:")
        print(result["error"])
 
    response = call("get_info")["result"]
 
    if "wallet_next_block_production_time" not in response or not response["wallet_next_block_production_time"]:
      print("Next production time is unset... Are there active delegates here?")
    if parse_date(response["ntp_time"]) - parse_date(response["blockchain_head_block_timestamp"]) > MAX_ALLOWABLE_HEAD_BLOCK_AGE:
      print("Head block is too old: %s" % (response["blockchain_head_block_age"]))
    if int(response["network_num_connections"]) < 1:
      print("No connections to delegate")
 
    time.sleep(10)
  except:
    try:
      sock = connect()
      call("login", [RPC_USERNAME, RPC_PASSWORD])
    except:
      pass
    time.sleep(10)
