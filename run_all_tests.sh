#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# Test 1
#########################################################

redis-cli flushall > /dev/null 2>&1
./menu_tool -o 4 menus/test1/cidades_small2:2:1 menus/test1/cidades_small2:2:1 | redis-cli -x set menu > /dev/null 2>&1
./server test.conf > /dev/null 2>&1 &
server_pid=$!

# Gives some time for the server to start before we begin to run the
# tests.
sleep 3

# This test must come first as it requires the client to receive a
# specific number of messages.
./publish_tests --test 1 --publishers 300 --listeners 20  --launch-interval 10

./publish_tests --test 3 --launch-interval 10

# It looks like this test is influenced by to first test and be run
# separately.
./publish_tests --test 2 --handshake-timeout 3
./publish_tests --test 4
./publish_tests --test 5

kill -9 $server_pid

