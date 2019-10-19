#!/bin/bash

if [ $# -ne 1 ]; then
   echo "Usage: $0 path"
   exit 1
fi

dir=$1
depth=3
version=3

menu0=${dir}/locations.txt:${depth}:${version}
menu1=${dir}/products.txt:${depth}:${version}
menu2=${dir}/ex_details.txt:${depth}:${version}
menu3=${dir}/in_details.txt:${depth}:${version}

occase-menu -o 4 $menu0 $menu1 > menu.txt
occase-menu -o 4 $menu0 > menu0.txt
occase-menu -o 4 $menu1 > menu1.txt
occase-menu -o 4 $menu2 > ex_details_menu.txt
occase-menu -o 4 $menu3 > in_details_menu.txt

