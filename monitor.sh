#!/bin/bash

host=127.0.0.1
port=9091

if [ $# -eq 1 ]; then
   host=$1;
elif [ $# -eq 2 ]; then
   host=$1;
   port=$2;
fi

echo "Host $host, port $port"

while true; do
   wget -O - $host:$port/stats 2> /dev/null
   sleep 1
done

