#!/bin/bash

while true; do
   wget -O - 127.0.0.1:9090/stats 2> /dev/null
   sleep 1
done

