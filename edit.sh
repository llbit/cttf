#!/bin/bash

interrupted()
{
	exit $?
}

trap interrupted SIGINT

if [ $# -lt "1" ]; then
	echo "usage: $0 id"
	exit 1
fi

if [ ! -e "tmp$1" ]; then
	echo "could not open tmp$1"
	exit 1
fi

i=$1
while true; do
	j=`expr $i + 1`
	./vex "tmp$i" > "tmp$j"
	./ftest -t "tmp$j"
	if [ $? -ne "0" ]; then
		echo "EDITING $j"
		i=$j
	fi
done
