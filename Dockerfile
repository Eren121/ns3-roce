FROM ubuntu:22.04

ARG GCC_VERSION="11"

# Ubuntu 18.04 is the last version of Ubuntu to have gcc5 available in apt
# Python 2 is default on Ubuntu 18.04, what we want
RUN apt update \
 && apt install -y gcc-${GCC_VERSION} g++-${GCC_VERSION} gdb python2 python3

# Set default C/C++ compilers
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100 \
 && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc   30 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++   30

RUN update-alternatives --config g++ \
 && update-alternatives --config gcc \ 
 && update-alternatives --config cc  \
 && update-alternatives --config c++

WORKDIR /app