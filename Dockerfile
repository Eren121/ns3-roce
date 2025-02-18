FROM ubuntu:24.04

# https://askubuntu.com/questions/909277/avoiding-user-interaction-with-tzdata-when-installing-certbot-in-a-docker-contai
ARG DEBIAN_FRONTEND=noninteractive

RUN apt update \
 && apt install -y gcc g++ gdb cmake python3

# Download necessary stuff to build netanim
RUN apt install -y mercurial qtbase5-dev qt5-qmake make

# Necessary stuff for analysis
RUN apt install -y \
  python3-jinja2 \
  python3-seaborn \
  python3-joblib \
  python3-git \
  python3-rich


# BUild reflect-cpp
RUN git clone https://github.com/getml/reflect-cpp --branch v0.17.0 /reflectcpp

WORKDIR /reflectcpp

RUN cmake -S . -B build -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  && cmake --build build \
  && cmake --build build --target install \
  && cmake --build build --target clean

RUN cmake -S . -B build -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  && cmake --build build \
  && cmake --build build --target install \
  && cmake --build build --target clean

WORKDIR /app