#!/bin/sh

grep "^tim 5" log.txt |awk '{print $3-N3" "($4-N4)*1000+($5-N5)/1e6; N3=$3;N4=$4;N5=$5;}' | tail -n +2 |plot.pl -nox
