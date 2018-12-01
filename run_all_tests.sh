#!/bin/bash

./menu_dump --menu 1 --sim-length 5 | redis-cli -x set menu

./read_only_tests --users 100 \
                  --launch-interval 10 \
                  --handshake-test-size 100

#./reg_users_tests -u $users -g $launch_interval

./publish_tests --publish-users 20 \
                --listen-users 20 \
                --simulations 3 \
                --msgs-per-channel 5
