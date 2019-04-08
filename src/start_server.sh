#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menus/cidades:2:1 menus/fipe_veiculos.txt:2:1 | redis-cli -x set menu
./server menu_chat_server.conf

