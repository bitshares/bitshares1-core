#!/usr/bin/env python3

import json
import sys

# run multest > multest.out first, check results

m128 = (1 << 128) - 1

for line in sys.stdin:
    u = json.loads(line)
    u = [int(e, 16) for e in u]
    a = u[0]
    b = u[1]
    ab = a*b
    abm = ab & m128
    v = [a, b, abm, ab >> 128, abm]
    if u != v:
        print("problem:", a, b)
        break
