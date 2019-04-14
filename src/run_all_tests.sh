#!/bin/bash

# Test 1
#########################################################

redis-cli flushall
./menu_dump -o 4 menus/cidades_small2:2:1 menus/cidades_small2:2:1 | redis-cli -x set menu
./server test.conf > /dev/null 2>&1 &
server_pid=$!

# Gives some time for the server to start before we begin to run the
# tests.
sleep 3

./read_only_tests --users 100 --launch-interval 10 --handshake-test-size 100

#./reg_users_tests -u $users -g $launch_interval

./publish_tests --publishers 300 --listeners 20 --type 1

kill -9 $server_pid

# Test 2
#########################################################

./publish_tests --type 2 --handshake-timeout 3

