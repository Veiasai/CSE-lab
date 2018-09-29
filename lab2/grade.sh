#!/bin/bash
make clean &> /dev/null 2>&1
make &> /dev/null 2>&1 
./part1_tester

./part2_tester.sh
