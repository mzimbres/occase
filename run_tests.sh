#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# First we setup redis for the test.
menu_dir=menus/test1
menu_depth=3
menu_version=3
menu0=${menu_dir}/locations.txt:${menu_depth}:${menu_version}
menu1=${menu_dir}/products.txt:${menu_depth}:${menu_version}

redis-cli flushall
./menu_tool -o 4 $menu0 $menu1 | redis-cli -x set menu

# We start the server and put it on the background. Consider starting
# many server instances.
./server test.conf > /dev/null 2>&1 &
server_pid=$!

# Gives some time for the server to start before we begin to run the
# tests.
sleep 3

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./publish_tests --test 1\
                   --publishers 1\
                   --listeners 1\
                   --post-listeners 5\
                   --menu menus/test1/json.txt\
                   --launch-interval 10 > /dev/null
done

for (( i = 0 ; i < 2 ; ++i )); do
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

