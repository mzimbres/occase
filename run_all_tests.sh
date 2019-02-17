#!/bin/bash

redis-cli flushall
./menu_dump -o 6 menus/cidades_small:2:1 menus/cidades_small:2:1 | redis-cli -x set menu

./read_only_tests --users 100 \
                  --launch-interval 10 \
                  --handshake-test-size 100

#./reg_users_tests -u $users -g $launch_interval

./publish_tests --publishers 2 \
                --listeners 2 \
                --simulations 2 \
                --handshake-timeout 3
