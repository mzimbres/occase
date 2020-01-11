#!/bin/bash

# NOTE: Be aware, that the max number of sub channels supported is 64.

redis-cli flushall
cat tests/channels.txt | redis-cli -x set channels

# We start the server and put it on the background. Consider starting
# many server instances.
./occase-db tests/test.conf
