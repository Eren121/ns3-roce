#!/bin/bash

cd $(dirname "$0") || exit 1

if [[ ! -d "netanim" ]]; then
    hg clone http://code.nsnam.org/netanim || exit 1
else
    echo "netanim folder already existing, skipping clone"
fi

cd netanim
hg up netanim-3.108
qmake NetAnim.pro && make clean && make