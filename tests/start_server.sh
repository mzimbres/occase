#!/bin/bash

redis-cli flushall

# TODO: Consider starting many server instances.
./occase-db tests/test.conf
