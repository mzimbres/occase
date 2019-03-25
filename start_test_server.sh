#!/bin/bash

redis-cli flushall
./menu_dump -o 4 menus/cidades_small:2:1 menus/cidades_small:2:1 | redis-cli -x set menu
./server

