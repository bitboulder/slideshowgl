#!/bin/sh


DIR="/home/src/self/slideshowgl/slideshowgl-2.0.0"
CDIR="$DIR/core_dump"

SL="$DIR/build/slideshowgl.online"

#cd $CDIR
ulimit -c unlimited

$SL $*

if [ -f "core" ]; then
	TIME=`date +%Y%m%d_%H%M%S`
	TIME="$CDIR/$TIME"
	mkdir $TIME
	mv core $TIME
	cd $TIME
	cp $SL .
	svn info $DIR >svn-info
	svn diff $DIR >svn-diff
	echo $* >arg
fi
