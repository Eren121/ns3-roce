#!/bin/bash

# Exit on error.
set -e

# cd to this bash script directory.
cd $(dirname "$0")

repo_local_path="netanim-src"
cmake_build_path="netanim-build"
cmake_configure_vars="-DCMAKE_BUILD_TYPE=Release"

# Clone netanim repo.
if [[ ! -d "netanim" ]]; then
    git clone https://github.com/Eren121/netanim-roce "$repo_local_path"
else
    echo "netanim folder already existing, skipping clone"
fi

# git checkout netanim-3.108

# Build netanim.
cmake $cmake_configure_vars -S "$repo_local_path" -B "$cmake_build_path"
cmake --build "$cmake_build_path"
