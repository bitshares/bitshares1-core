#pragma once

#define BTS_MAIL_INVENTORY_FETCH_LIMIT 4096
#define BTS_MAIL_MAX_MESSAGE_SIZE_BYTES (1024*1024)
#define BTS_MAIL_MAX_MESSAGE_AGE (fc::minutes(5))
#define BTS_MAIL_PROOF_OF_WORK_TARGET (fc::ripemd160("000000fffdeadbeeffffffffffffffffffffffff"))
#define BTS_MAIL_DEFAULT_MAIL_SERVERS (std::unordered_set<fc::ip::endpoint>({fc::ip::endpoint(fc::ip::address("107.170.30.182"), 1800)}))
