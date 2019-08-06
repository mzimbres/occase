#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menus/vehicle/locations.txt:2:2 menus/vehicle/products.txt:3:3 | redis-cli -x set menu
./server menu-chat-server.conf

