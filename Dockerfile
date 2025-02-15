FROM ubuntu:22.04

# Ubuntu 18.04 is the last version of Ubuntu to have gcc5 available in apt
# Python 2 is default on Ubuntu 18.04, what we want
RUN apt update \
 && apt install -y gcc g++ gdb cmake python2 python3

# Download necessary stuff to build netanim
RUN apt install -y mercurial qtbase5-dev qt5-qmake make

WORKDIR /app