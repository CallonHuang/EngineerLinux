#!/bin/sh
safe_run()
{
    $@
    if [ $? -ne 0 ];then
        echo "build failed!"
        exit -1
    fi
}

rm -f main process
safe_run gcc main.c -o main
safe_run gcc process.c -o process