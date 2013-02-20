#!/bin/bash

# Test a number of characters in all fonts in the local directory with ftest
# $1 is the TTF file

make

runs=0
passed=0
for test in test/bug* test/test*; do
	echo "${test%.shape}"
	if ./ftest "$test" -t >/dev/null; then
		(( passed += 1 ))
	fi
	(( runs += 1 ))
done

echo "$passed/$runs"
