#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./publish_tests --test 1\
                   --publishers 1\
                   --listeners 1\
                   --post-listeners 5\
                   --menu menus/test1/json.txt\
                   --launch-interval 10
done

for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 3-$i"
   ./publish_tests --test 3\
                   --menu menus/test1/json.txt\
                   --launch-interval 10
done

echo "Starting test 2"
./publish_tests --test 2\
                --menu menus/test1/json.txt\
                --handshake-timeout 3

echo "Starting test 4"
./publish_tests --test 4\
                --menu menus/test1/json.txt

echo "Starting test 5"
./publish_tests --test 5\
                --menu menus/test1/json.txt

