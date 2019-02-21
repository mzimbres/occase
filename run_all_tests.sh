#!/bin/bash

./read_only_tests --users 100 \
                  --launch-interval 10 \
                  --handshake-test-size 100

#./reg_users_tests -u $users -g $launch_interval

./publish_tests --publishers 4 \
                --listeners 4 \
                --simulations 3 \
                --handshake-timeout 3
