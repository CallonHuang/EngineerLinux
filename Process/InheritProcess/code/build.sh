#!/bin/bash
gcc server.c -o server -lpthread
gcc client.c -o client
gcc subprocess.c -o subprocess