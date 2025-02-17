FROM ubuntu:22.04

# https://askubuntu.com/questions/909277/avoiding-user-interaction-with-tzdata-when-installing-certbot-in-a-docker-contai
ARG DEBIAN_FRONTEND=noninteractive

# Ubuntu 18.04 is the last version of Ubuntu to have gcc5 available in apt
# Python 2 is default on Ubuntu 18.04, what we want
RUN apt update \
 && apt install -y gcc g++ gdb cmake python2 python3

# Download necessary stuff to build netanim
RUN apt install -y mercurial qtbase5-dev qt5-qmake make

# Necessary stuff for analysis
RUN apt install -y \
  python3-jinja2 \
  python3-seaborn \
  python3-joblib \
  python3-git \
  python3-rich

WORKDIR /app