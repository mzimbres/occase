#!/bin/bash

# Be aware of the fact that there is a limit on the size of user
# pending messages, see redis-offline-chat-msgs
for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 1-a-$i"
   ./db_tests --test 1 --publishers 2 --listeners 100 --post-listeners 5 --launch-interval 10
done

for (( i = 0 ; i < 2 ; ++i )); do
   echo "Starting test 3-$i"
   ./db_tests --test 3 --launch-interval 10
done

echo "Starting test 4"
./db_tests --test 4

#echo "Starting test 5"
#./db_tests --test 5

