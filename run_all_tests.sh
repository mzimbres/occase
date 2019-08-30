#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# Test 1
#########################################################

redis-cli flushall
./load-tool menus/test1 3 1
./server test.conf > /dev/null 2>&1 &
server_pid=$!

# Gives some time for the server to start before we begin to run the
# tests.
sleep 3

./publish_tests --test 1\
                --publishers 1\
                --listeners 1\
                --post-listeners 100\
                --launch-interval 10

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
./publish_tests --test 1\
                --publishers 1\
                --listeners 100\
                --post-listeners 100\
                --launch-interval 10

# After the introduction of the delete command it should not be a
# problem to run test 1 many times.
./publish_tests --test 1\
                --publishers 300\
                --listeners 20\
                --launch-interval 100

./publish_tests --test 3 --launch-interval 10

# It looks like this test is influenced by to first test and be run
# separately.
./publish_tests --test 2 --handshake-timeout 3
./publish_tests --test 4
./publish_tests --test 5

kill -9 $server_pid

