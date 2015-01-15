
#include <iostream>

#include <fc/uint128.hpp>
#include <fc/crypto/sha256.hpp>

#ifndef WIN32
#ifndef __APPLE__
#include <time.h>
#endif
#endif

// get some entropy by running sha256 as a counter
class simple_rng
{
public:
    simple_rng( uint64_t seed = 0 )
    {
        this->seed = seed;
        return;
    }

    void next( fc::uint128 &a, fc::uint128 &b )
    {        
        fc::sha256 h = fc::sha256::hash( ((char* ) &seed), sizeof(seed) );
        this->seed++;
        a = fc::uint128( h._hash[3], h._hash[2] );
        b = fc::uint128( h._hash[1], h._hash[0] );
        return;
    }
    
    uint64_t seed;
};

void hex(const fc::uint128& x, char* buf)
{
    static char hex_digit[] = "0123456789abcdef";
    uint64_t a = x.high_bits();
    uint64_t b = x.low_bits();
    
    buf[0x20] = '\0';
    buf[0x1f] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x1e] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x1d] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x1c] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x1b] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x1a] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x19] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x18] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x17] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x16] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x15] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x14] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x13] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x12] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x11] = hex_digit[b & 0x0f]; b >>= 4;
    buf[0x10] = hex_digit[b & 0x0f];
    
    buf[0x0f] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x0e] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x0d] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x0c] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x0b] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x0a] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x09] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x08] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x07] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x06] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x05] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x04] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x03] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x02] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x01] = hex_digit[a & 0x0f]; a >>= 4;
    buf[0x00] = hex_digit[a & 0x0f];
    
    return;
}

int main(int argc, char** argv, char** envp)
{
    simple_rng rng;
    bool first = true;
    fc::uint128 b1_last = 0xFFFF;
    fc::uint128 total_time = 0;
    uint64_t calls = 0;
    
    for(int i=0;i<128;i++)
    {
        fc::uint128 a, b;
        rng.next( a, b );
        for(int a_mbits=0;a_mbits<128;a_mbits++)
        {
            fc::uint128 am = 1;
            am <<= a_mbits;
            am--;
            fc::uint128 a1 = a & am;
            for(int b_mbits=0;b_mbits<128;b_mbits++)
            {
                fc::uint128 bm = 1;
                bm <<= b_mbits;
                bm--;
                fc::uint128 b1 = b & bm;

                if( b1 == b1_last )
                    continue;

                if( first )
                {
                    a1 = 1;
                    b1 = 0;
                }

                fc::uint128 rh, rl;
                fc::uint128::full_product( a1, b1, rh, rl );
                
                if( first )
                    first = false;

                fc::uint128 a1b1 = a1;
#ifndef WIN32
#ifndef __APPLE__
                // report time
                timespec t_before;
                timespec t_after;
                
                clock_gettime( CLOCK_MONOTONIC, &t_before );
#endif
#endif
                a1b1 *= b1;
#ifndef WIN32
#ifndef __APPLE__
                clock_gettime( CLOCK_MONOTONIC, &t_after );
                fc::uint128 ns_before;
                fc::uint128 ns_after;

                ns_before = fc::uint128(uint64_t(t_before.tv_sec));
                ns_before *= 1000000000;
                ns_before += fc::uint128(uint64_t(t_before.tv_nsec));

                ns_after = fc::uint128(uint64_t(t_after.tv_sec));
                ns_after *= 1000000000;
                ns_after += fc::uint128(uint64_t(t_after.tv_nsec));
                
                total_time += (ns_after - ns_before);
                calls++;
#endif
#endif

                char data[5][33];
                hex(a1, data[0]);
                hex(b1, data[1]);
                hex(a1b1, data[2]);
                hex(rh, data[3]);
                hex(rl, data[4]);

                std::cout << " [\"" << data[0]
                             << "\", \"" << data[1]
                             << "\", \"" << data[2]
                             << "\", \"" << data[3]
                             << "\", \"" << data[4]
                             << "\"]\n";
                b1_last = b1;
            }
        }
    }
    char h_tt[33];
    hex(total_time, h_tt);
    std::cerr << "total_time: 0x" << h_tt << " (ns)\n";
    std::cerr << "calls:" << calls << "\n";
    std::cerr << "\n";
    return 0;
}
