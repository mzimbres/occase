#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menu2/locations.txt:2:2 menu2/products.txt:3:3 | redis-cli -x set menu
./server menu_chat_server.conf

