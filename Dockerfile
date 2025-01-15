FROM ubuntu:18.04
# Ubuntu 18.04 is the last version of Ubuntu to have gcc5 available in apt
# Python 2 is default on Ubuntu 18.04, what we want
RUN apt update && apt install -y gcc-5 g++-5 gdb python python3

# Set gcc5 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 100 \
 && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc   30  \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++   30

RUN update-alternatives --config g++ \
 && update-alternatives --config gcc \ 
 && update-alternatives --config cc  \
 && update-alternatives --config c++     

WORKDIR /app