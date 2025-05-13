#!/bin/bash

# Exit on error.
set -e

# cd to this bash script directory.
cd $(dirname "$0")

# Clone netanim repo.
if [[ ! -d "netanim" ]]; then
    git clone https://gitlab.com/nsnam/netanim.git
else
    echo "netanim folder already existing, skipping clone"
fi

# Build netanim.
cd netanim
git checkout netanim-3.108
qmake NetAnim.pro && make clean && make
