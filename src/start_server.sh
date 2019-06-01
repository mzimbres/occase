#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menu1/locations.txt:2:1 menu1/products.txt:2:1 | redis-cli -x set menu
./server menu_chat_server.conf

