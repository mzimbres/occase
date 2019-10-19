#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./tests --test 1\
           --publishers 1\
           --listeners 1\
           --post-listeners 5\
           --channels menus/test1/channels.txt\
           --filters menus/test1/filters.txt\
           --launch-interval 10
done

for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 3-$i"
   ./tests --test 3\
           --channels menus/test1/channels.txt\
           --filters menus/test1/filters.txt\
           --launch-interval 10
done

echo "Starting test 2"
./tests --test 2\
        --channels menus/test1/channels.txt\
        --filters menus/test1/filters.txt\
        --handshake-timeout 3

echo "Starting test 4"
./tests --test 4 \
        --channels menus/test1/channels.txt\
        --filters menus/test1/filters.txt

echo "Starting test 5"
./tests --test 5 \
        --channels menus/test1/channels.txt\
        --filters menus/test1/filters.txt

