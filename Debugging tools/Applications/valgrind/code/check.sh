#!/bin/sh

SH_CUR_DIR=$(dirname `readlink -f $0`)

cd $SH_CUR_DIR/
g++ -L./ test.cpp -o test -lmemtrace -lc -lstdc++ -lpthread
if [ $? -ne 0 ];then
    echo "test compile failed!"
    cd -
    exit 1
fi

valgrind=`which valgrind`
ret=0
if [ ! -n "$valgrind" ];then
	echo "valgrind env is not correct!"
	cd -
	exit 1
else
	rm -f trace.log report.log
	$valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=./report.log ./test > trace.log &
	count=0
	while [ $count -le 15 ]
	do
		sleep 1
		if [ -f report.log ];then
			pid=`cat report.log | grep "Copyright" | awk -F '==' '{print $2}'`
			if grep 'normal_occupy=' trace.log;then
				#END OF TEST
				normal_occupy=`cat trace.log | grep "normal_occupy="`
				normal_occupy=${normal_occupy##*=}
				break
			fi
		fi
		let count++
	done
	if [ $count -ge 15 ];then
		echo "may jammed!"
		ret=1
	fi
	
	if [ -n "$pid" ];then
		echo "pid="$pid
		ps_ret=`ps | grep "$pid"`
		echo $ps_ret
		if [ -n "$ps_ret" ];then
			echo "need kill $pid"
			kill -9 $pid
		fi
	fi
	if [ $ret -eq 0 ];then
		eval $(sed 's/,//g' report.log | awk '
		BEGIN{definitely_lost=0;indirectly_lost=0;possibly_lost=0;still_reachable=0;tmp=0;invalid=0;}
		/Invalid write/{
			invalid=1
		}
		/Invalid read/{
			invalid=1
		}
		/definitely lost:/{
			definitely_lost=$4
		}
		/indirectly lost:/{
			indirectly_lost=$4
		}
		/possibly lost:/{
			possibly_lost=$4
		}
		/still reachable in loss/{
			tmp=$2
			still_reachable+=tmp
		}
		/allocatestack.c:/{
			still_reachable-=tmp
		}

		END{
		print "invalid="invalid
		print "definitely_lost="definitely_lost
		print "indirectly_lost="indirectly_lost
		print "possibly_lost="possibly_lost
		print "still_reachable="still_reachable
		}
		')
		
		if [ ! -n "$normal_occupy" ];then
			normal_occupy=0
		fi
		echo "invalid=$invalid normal_occupy=$normal_occupy, still_reachable=$still_reachable, definitely_lost=$definitely_lost, indirectly_lost=$indirectly_lost, possibly_lost=$possibly_lost"
		if [ $definitely_lost -eq 0 ] && [ $indirectly_lost -eq 0 ] && [ $possibly_lost -eq 0 ];then
			if [ $still_reachable -eq $normal_occupy ] && [ $invalid -eq 0 ];then
				ret=0
			else
				ret=1
			fi
		else
			ret=1
		fi
	fi
	if grep 'heap_over_check' trace.log;then
		echo "demo checked heap over!"
		ret=1
	fi
fi
cd -
exit $ret


