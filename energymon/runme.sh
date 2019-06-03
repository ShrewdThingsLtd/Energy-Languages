#!/bin/bash

ROOTDIR=$PWD

g++ -g $ROOTDIR/energymon/energymon.cpp -leml -o $ROOTDIR/energymon/energymon
$ROOTDIR/energymon/energymon
