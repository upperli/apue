#!/usr/bin/bash


buffsize=1
echo -e "缓冲大小\trealtime\tusertime\tsystime"
for (( i = 1; i < 20; i++ )); do
	time=""
	for tmp in `(time ./main $buffsize < testfile > testfile2) 2>&1 | cut  -f 2`;do
		time="$time\t$tmp"
	done
	echo -e "$buffsize\t$time"
	((buffsize=$buffsize * 2))
done