#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./db_tests --test 1\
              --publishers 1\
              --listeners 1\
              --post-listeners 5\
              --channels tests/channels.txt\
              --filters tests/filters.txt\
              --launch-interval 10
done

for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 3-$i"
   ./db_tests --test 3\
              --channels tests/channels.txt\
              --filters tests/filters.txt\
              --launch-interval 10
done

echo "Starting test 2"
./db_tests --test 2\
           --channels tests/channels.txt\
           --filters tests/filters.txt\
           --handshake-timeout 3

echo "Starting test 4"
./db_tests --test 4 \
           --channels tests/channels.txt\
           --filters tests/filters.txt

echo "Starting test 5"
./db_tests --test 5 \
           --channels tests/channels.txt\
           --filters tests/filters.txt

