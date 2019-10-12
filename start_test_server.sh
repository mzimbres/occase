#!/bin/bash

# NOTE: Be aware of the fact that the max number of items in the first
# menu that can be subscribed to is 16, therefore the first menu has
# to have at most that number of entries at its depth for the test to
# work.

# First we setup redis for the test.
menu_dir=menus/test1
menu_depth=3
menu_version=3
menu0=${menu_dir}/locations.txt:${menu_depth}:${menu_version}
menu1=${menu_dir}/products.txt:${menu_depth}:${menu_version}

redis-cli flushall
./menu_tool -o 4 $menu0 $menu1 | redis-cli -x set menu

# We start the server and put it on the background. Consider starting
# many server instances.
./db test.conf
