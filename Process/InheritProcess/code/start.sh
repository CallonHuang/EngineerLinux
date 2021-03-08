#!/bin/bash
process=("server" "subprocess" "client")
cleanup()
{
    echo get signal
    for element in ${process[@]}
    do
        pid=`pgrep "$element"`
        if [ ! -z $pid ];then
            kill -s 9 $pid
        fi
        rm -f $element
    done
    trap - INT
    kill -s INT "$$"
}
trap "cleanup" INT 
./server &
./client &
while true
do
	sleep 5
done


