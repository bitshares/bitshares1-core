#!/usr/bin/env bash

sudo launchctl limit maxfiles 1000000 1000000
ulimit -n 100000

for regression_test in `ls regression_tests | grep -v "^\\_"`; do
    printf " Running test $regression_test...                            \\r"
    ./wallet_tests -t regression_tests_without_network/$regression_test > /dev/null 2>&1 || printf "Test $regression_test failed.                            \\n"
    sleep 2
done;
printf "                                                                            \\r"
