#!/bin/sh
cd libbacktrace
rm -rf *.so
gcc -fPIC -shared -D_GNU_SOURCE backtrace.c -o libbacktrace.so -ldl
cd -
rm -f main
gcc -funwind-tables -rdynamic -L./libbacktrace main.c -o main -lbacktrace
