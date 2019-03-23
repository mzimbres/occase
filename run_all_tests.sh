#!/bin/bash

./read_only_tests --users 100 \
                  --launch-interval 10 \
                  --handshake-test-size 100

#./reg_users_tests -u $users -g $launch_interval

./publish_tests --publishers 1 \
                --listeners 100 \
                --simulations 3 \
                --handshake-timeout 3
