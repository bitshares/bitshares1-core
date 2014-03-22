#pragma once

//TODO obviously both temp
//need to be able to keep these low when in test mode
#define DNS_AUCTION_DURATION_BLOCKS (3)
#define DNS_EXPIRE_DURATION_BLOCKS  (8)

#define BTS_DNS_MIN_BID_FROM(bid)   ((11 * (bid)) / 10)
#define BTS_DNS_BID_FEE_RATIO(bid)  ((bid) / 2)

#define BTS_DNS_MAX_NAME_LEN        (128)
#define BTS_DNS_MAX_VALUE_LEN       (1024)
