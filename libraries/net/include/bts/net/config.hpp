#pragma once

#define BTS_NET_PROTOCOL_VERSION 103 // protocol version to be used in testnet3

/** 
 * Define this to enable debugging code in the p2p network interface.
 * This is code that would never be executed in normal operation, but is
 * used for automated testing (creating artificial net splits, 
 * tracking where messages came from and when)
 */
#define ENABLE_P2P_DEBUGGING_API 1

/**
 * 512 kb
 */
#define MAX_MESSAGE_SIZE (524288)  
#define BTS_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME  (30) // seconds

/**
 * AFter trying all peers, how long to wait before we check to
 * see if there are peers we can try again.
 */
#define BTS_PEER_DATABASE_RETRY_DELAY  (15) // seconds

#define BTS_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT (5) 
#define BTS_NET_PEER_INACTIVITY_TIMEOUT (45) 
#define BTS_NET_PEER_DISCONNECT_TIMEOUT (20) 
