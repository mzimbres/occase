#!/bin/bash

# Test 1
#########################################################

redis-cli flushall > /dev/null 2>&1
./menu_dump -o 4 menus/cidades_small2:2:1 menus/cidades_small2:2:1 | redis-cli -x set menu > /dev/null 2>&1
./server test.conf > /dev/null 2>&1 &
server_pid=$!

# Gives some time for the server to start before we begin to run the
# tests.
sleep 3

./publish_tests --test 1 --publishers 300 --listeners 20
./publish_tests --test 3 --launch-interval 10
./publish_tests --test 2 --handshake-timeout 3
./publish_tests --test 4

kill -9 $server_pid

