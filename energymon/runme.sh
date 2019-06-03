#!/bin/bash

ROOTDIR=$PWD
EMLDIR=$PWD/../eml

gcc -g $ROOTDIR/energymon/energymon.cpp -std=c++11 -I$EMLDIR/include -leml -o $ROOTDIR/energymon/energymon
$ROOTDIR/energymon/energymon
