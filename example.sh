#!/bin/sh
g++ -g example.cpp -Iinclude -Llib -ljemalloc -o example
export LD_LIBRARY_PATH=lib
./example
./example -r
rm -f /tmp/app.mmap
rm -f /tmp/app.back
