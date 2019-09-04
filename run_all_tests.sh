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

for (( i = 0 ; i < 1 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./publish_tests --test 1\
                   --publishers 1\
                   --listeners 1\
                   --post-listeners 100\
                   --menu menus/test1/json.txt\
                   --launch-interval 10 > /dev/null
done

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 1 ; ++i )); do
   echo "Starting test 1-b-$i"
   ./publish_tests --test 1\
                   --publishers 1\
                   --listeners 100\
                   --post-listeners 100\
                   --menu menus/test1/json.txt\
                   --launch-interval 10 > /dev/null
done

for (( i = 0 ; i < 1 ; ++i )); do
   echo "Starting test 1-c-$i"
   ./publish_tests --test 1\
                   --publishers 3\
                   --listeners 2\
                   --post-listeners 100\
                   --menu menus/test1/json.txt\
                   --launch-interval 10 > /dev/null
done

for (( i = 0 ; i < 1 ; ++i )); do
   echo "Starting test 3-$i"
   ./publish_tests --test 3\
                   --menu menus/test1/json.txt\
                   --launch-interval 10 > /dev/null
done

echo "Starting test 2"
./publish_tests --test 2\
                --menu menus/test1/json.txt\
                --handshake-timeout 3 > /dev/null

echo "Starting test 4"
./publish_tests --test 4\
                --menu menus/test1/json.txt > /dev/null

echo "Starting test 5"
./publish_tests --test 5\
                --menu menus/test1/json.txt >/dev/null

kill -9 $server_pid

