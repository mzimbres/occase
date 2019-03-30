#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menus/cidades_small:2:1 menus/cidades_small:2:1 | redis-cli -x set menu
./server menu_chat_server.conf &
server_pid=$!
sleep 3
kill -9 $server_pid

