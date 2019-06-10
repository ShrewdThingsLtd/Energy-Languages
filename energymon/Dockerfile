FROM ubuntu:18.10
MAINTAINER erez@shrewdthings.com

ARG GIT_REPO='https://github.com/HPC-ULL/eml.git'
ARG GIT_BRANCH='master'

# eml dependencies:
RUN apt-get -y update && apt-get -y install msrtool automake libtool libconfuse2 libconfuse-common libconfuse-dev pkg-config
RUN apt-get -y update && apt-get -y install git

WORKDIR /usr/local/src
RUN git clone -b $GIT_BRANCH $GIT_REPO
WORKDIR /usr/local/src/eml
RUN autoreconf -i
RUN ./configure && make && make install

# energymon dependencies:
RUN apt-get -y update && apt-get -y install libcurl4-openssl-dev

COPY ./src /usr/local/src/energymon
WORKDIR /usr/local/src
RUN g++ -g -std=c++11 ./energymon/energymon.cpp -leml -lcurl -o /usr/local/bin/energymon

