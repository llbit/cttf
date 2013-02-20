#!/bin/bash

# Test a number of characters in all fonts in the local directory with ftest
# $1 is the TTF file

make

for tf in *.ttf; do
	for c in a; do
	#for c in a b c d e f g h i j k l m n o p q r s t u v w x y z; do
		echo "./ftest $tf $c -t"
		./ftest "$tf" $c -t >/dev/null
	done
done
