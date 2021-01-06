#!/bin/bash

# NOTE: Be aware, that the max number of sub channels supported is 64.

redis-cli -p 1100 flushall # post
redis-cli -p 1102 flushall # msgs

# Consider starting many server instances.
./occase-db tests/test.conf
